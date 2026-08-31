/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file gc032a.cpp
 * @brief GC032A sensor driver.
 *
 * Register tables and control logic adapted from esp32-camera's gc032a.c
 * (Apache-2.0). The GC032A has no JPEG encoder, so
 * set_pixformat(PIXFORMAT_JPEG) always fails; the core also rejects it
 * up front via camera_sensor_info_t.support_jpeg.
 *
 * Framesize scaling uses windowing: crop the requested window out of the
 * full VGA frame (esp32-camera's gc032a.c has no subsample mode).
 */
#include "gc032a.h"

#include "pico/stdlib.h"

#include "../driver/sccb.h"
#include "gc032a_regs.h"
#include "gc032a_settings.h"

#define H8(v) ((v)>>8)
#define L8(v) ((v)&0xff)

// ---------------------------------------------------------------------------
// Low-level register access
// ---------------------------------------------------------------------------

static int reg_write(uint8_t reg, uint8_t value) {
    return sccb_write8(GC032A_SCCB_ADDR, reg, value);
}

static int reg_read(uint8_t reg) {
    uint8_t value = 0;
    if (sccb_read8(GC032A_SCCB_ADDR, reg, &value) != 0) {
        return -1;
    }
    return value;
}

static int set_reg_bits(uint8_t reg, uint8_t offset, uint8_t mask, uint8_t value) {
    int old = reg_read(reg);
    if (old < 0) {
        return old;
    }
    uint8_t new_value = (uint8_t)((old & ~(mask << offset)) | ((value & mask) << offset));
    return reg_write(reg, new_value);
}

// Register list terminated by REGLIST_TAIL; REG_DLY pauses value milliseconds.
static int regs_write(const uint16_t (*regs)[2]) {
    for (size_t i = 0; regs[i][0] != REGLIST_TAIL; i++) {
        if (regs[i][0] == REG_DLY) {
            sleep_ms(regs[i][1]);
        } else if (reg_write((uint8_t)regs[i][0], (uint8_t)regs[i][1]) != 0) {
            return -1;
        }
    }
    return 0;
}

// ---------------------------------------------------------------------------
// sensor_t interface
// ---------------------------------------------------------------------------

// Host-specific tweaks, applied after the default table:
// - PLL/DVP clock division (same register pokes esp32-camera's gc032a
//   reset() does on ESP32): the RP2040 capture path needs the headroom
//   too - at full PCLK bytes are dropped and the image shows noise
//   streaks (same failure mode as on the GC2145).
// - P0_SYNC_MODE bit0: VSYNC polarity. esp32-camera leaves it set because
//   the ESP32 camera peripheral inverts VSYNC in hardware; the PIO capture
//   engine expects VSYNC high during the frame, so keep it cleared.
static int gc032a_apply_host_tweaks(void) {
    int ret = 0;
    ret |= reg_write(RESET_RELATED, 0x00);          // page 0
    ret |= set_reg_bits(PLL_MODE1, 1, 0x01, 1);     // PLL_mode1: div2en
    ret |= set_reg_bits(PLL_MODE1, 7, 0x01, 1);     // PLL_mode1: dvp mode
    ret |= set_reg_bits(PLL_MODE2, 0, 0x3f, 8);     // PLL_mode2: divx4
    ret |= set_reg_bits(ISP_DIV_MODE, 4, 0x0f, 2);  // clk div mode: divide_by
    ret |= set_reg_bits(P0_SYNC_MODE, 0, 0x01, 0);  // VSYNC high-valid
    return ret;
}

static int gc032a_reset(sensor_t *sensor) {
    (void)sensor;
    // Software reset: clear all registers back to their default values
    if (reg_write(RESET_RELATED, 0xf0) != 0) {
        return -1;
    }
    sleep_ms(100);
    if (regs_write(gc032a_default_init_regs) != 0) {
        return -1;
    }
    gc032a_apply_host_tweaks();
    sleep_ms(100);
    return 0;
}

static int gc032a_set_pixformat(sensor_t *sensor, pixformat_t pixformat) {
    (void)sensor;
    int ret;
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        reg_write(RESET_RELATED, 0x00);  // page 0
        ret = set_reg_bits(0x44, 0, 0x1f, 6);  // RGB565
        break;
    default:
        return -1;  // GC032A has no JPEG encoder
    }
    return ret;
}

static int gc032a_set_framesize(sensor_t *sensor, framesize_t framesize) {
    (void)sensor;
    int ret = 0;
    if (framesize > FRAMESIZE_VGA) {
        framesize = FRAMESIZE_VGA;
    }
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;
    uint16_t row_s = (resolution[FRAMESIZE_VGA].height - h) / 2;
    uint16_t col_s = (resolution[FRAMESIZE_VGA].width - w) / 2;

    ret |= reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= reg_write(P0_ROW_START_HIGH, H8(row_s));
    ret |= reg_write(P0_ROW_START_LOW, L8(row_s));
    ret |= reg_write(P0_COLUMN_START_HIGH, H8(col_s));
    ret |= reg_write(P0_COLUMN_START_LOW, L8(col_s));
    ret |= reg_write(P0_WINDOW_HEIGHT_HIGH, H8(h + 8));
    ret |= reg_write(P0_WINDOW_HEIGHT_LOW, L8(h + 8));
    ret |= reg_write(P0_WINDOW_WIDTH_HIGH, H8(w + 8));
    ret |= reg_write(P0_WINDOW_WIDTH_LOW, L8(w + 8));

    ret |= reg_write(P0_WIN_MODE, 0x01);
    ret |= reg_write(P0_OUT_WIN_HEIGHT_HIGH, H8(h));
    ret |= reg_write(P0_OUT_WIN_HEIGHT_LOW, L8(h));
    ret |= reg_write(P0_OUT_WIN_WIDTH_HIGH, H8(w));
    ret |= reg_write(P0_OUT_WIN_WIDTH_LOW, L8(w));
    return ret;
}

static int gc032a_set_hmirror(sensor_t *sensor, int enable) {
    (void)sensor;
    int ret = reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= set_reg_bits(P0_CISCTL_MODE1, 0, 0x01, enable != 0);
    return ret;
}

static int gc032a_set_vflip(sensor_t *sensor, int enable) {
    (void)sensor;
    int ret = reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= set_reg_bits(P0_CISCTL_MODE1, 1, 0x01, enable != 0);
    return ret;
}

static int gc032a_set_colorbar(sensor_t *sensor, int enable) {
    (void)sensor;
    int ret = reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= set_reg_bits(P0_DEBUG_MODE2, 3, 0x01, enable != 0);
    return ret;
}

// Masked register access (8-bit only; mask > 0xFF unsupported here)
static int gc032a_set_reg(sensor_t *sensor, int reg, int mask, int value) {
    (void)sensor;
    if (mask > 0xFF) {
        return -1;
    }
    int old = reg_read((uint8_t)reg);
    if (old < 0) {
        return old;
    }
    value = (old & ~mask) | (value & mask);
    return reg_write((uint8_t)reg, (uint8_t)value);
}

static int gc032a_get_reg(sensor_t *sensor, int reg, int mask) {
    (void)sensor;
    if (mask > 0xFF) {
        return -1;
    }
    int ret = reg_read((uint8_t)reg);
    if (ret > 0) {
        ret &= mask;
    }
    return ret;
}

// ---------------------------------------------------------------------------
// Detection / registration
// ---------------------------------------------------------------------------

int gc032a_detect(sensor_t *sensor) {
    sensor->sccb_addr = GC032A_SCCB_ADDR;

    int midh = reg_read(SENSOR_ID_HIGH);
    int midl = reg_read(SENSOR_ID_LOW);
    uint16_t pid = (uint16_t)(midh << 8 | midl);
    if (pid != GC032A_PID) {
        // one retry, bus may still be settling
        sleep_ms(10);
        midh = reg_read(SENSOR_ID_HIGH);
        midl = reg_read(SENSOR_ID_LOW);
        pid = (uint16_t)(midh << 8 | midl);
    }
    if (pid != GC032A_PID) {
        return -1;
    }
    sensor->id.PID = pid;
    return 0;
}

int gc032a_init_sensor(sensor_t *sensor) {
    sensor->reset            = gc032a_reset;
    sensor->init_status      = NULL; // not ported yet
    sensor->set_pixformat    = gc032a_set_pixformat;
    sensor->set_framesize    = gc032a_set_framesize;
    sensor->set_brightness   = NULL; // not ported yet
    sensor->set_contrast     = NULL; // not ported yet
    sensor->set_saturation   = NULL; // not ported yet
    sensor->set_sharpness    = NULL; // not ported yet
    sensor->set_gainceiling  = NULL; // not ported yet
    sensor->set_quality      = NULL; // no JPEG, no quality setting
    sensor->set_colorbar     = gc032a_set_colorbar;
    sensor->set_whitebal     = NULL; // not ported yet
    sensor->set_gain_ctrl    = NULL; // not ported yet
    sensor->set_exposure_ctrl = NULL; // not ported yet
    sensor->set_hmirror      = gc032a_set_hmirror;
    sensor->set_vflip        = gc032a_set_vflip;
    sensor->set_special_effect = NULL; // not ported yet
    sensor->set_wb_mode      = NULL; // not ported yet
    sensor->set_ae_level     = NULL; // not ported yet
    sensor->set_aec_value    = NULL; // not ported yet
    sensor->set_reg          = gc032a_set_reg;
    sensor->get_reg          = gc032a_get_reg;
    sensor->set_xclk         = NULL;

    // Default register set
    regs_write(gc032a_default_init_regs);
    gc032a_apply_host_tweaks();
    sleep_ms(100);
    return 0;
}
