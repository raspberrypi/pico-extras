/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file gc0308.cpp
 * @brief GC0308 sensor driver.
 *
 * Register tables and control logic adapted from esp32-camera's gc0308.c
 * (Apache-2.0). The GC0308 has no JPEG encoder, so
 * set_pixformat(PIXFORMAT_JPEG) always fails; the core also rejects it
 * up front via camera_sensor_info_t.support_jpeg.
 *
 * Framesize scaling uses the subsample strategy (esp32-camera's Kconfig
 * default, CONFIG_GC_SENSOR_SUBSAMPLE_MODE): keep the widest field of
 * view and pick the coarsest subsample ratio that still covers the
 * requested window.
 */
#include "gc0308.h"

#include "pico/stdlib.h"

#include "../driver/sccb.h"
#include "gc0308_regs.h"
#include "gc0308_settings.h"

#define H8(v) ((v)>>8)
#define L8(v) ((v)&0xff)

// ---------------------------------------------------------------------------
// Low-level register access
// ---------------------------------------------------------------------------

static int reg_write(uint8_t reg, uint8_t value) {
    return sccb_write8(GC0308_SCCB_ADDR, reg, value);
}

static int reg_read(uint8_t reg) {
    uint8_t value = 0;
    if (sccb_read8(GC0308_SCCB_ADDR, reg, &value) != 0) {
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
// - PCLK_DIV bits[6:4]: divide PCLK down. esp32-camera applies this on ESP32
//   to keep pclk <= 15 MHz; the RP2040 capture path needs the headroom too
//   (at full PCLK bytes are dropped and the image shows noise streaks - same
//   failure mode as on the GC2145).
static int gc0308_apply_host_tweaks(void) {
    int ret = 0;
    ret |= reg_write(RESET_RELATED, 0x00);          // page 0
    ret |= set_reg_bits(PCLK_DIV, 4, 0x07, 1);      // divide PCLK
    return ret;
}

static int gc0308_reset(sensor_t *sensor) {
    (void)sensor;
    // Software reset: clear all registers back to their default values
    if (reg_write(RESET_RELATED, 0xf0) != 0) {
        return -1;
    }
    sleep_ms(80);
    if (regs_write(gc0308_default_init_regs) != 0) {
        return -1;
    }
    gc0308_apply_host_tweaks();
    sleep_ms(80);
    return 0;
}

static int gc0308_set_pixformat(sensor_t *sensor, pixformat_t pixformat) {
    (void)sensor;
    int ret;
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        reg_write(RESET_RELATED, 0x00);  // page 0
        ret = set_reg_bits(OUTPUT_FMT, 0, 0x0f, 6);  // RGB565
        break;
    case PIXFORMAT_YUV422:
        reg_write(RESET_RELATED, 0x00);  // page 0
        ret = set_reg_bits(OUTPUT_FMT, 0, 0x0f, 2);  // YUV422, Y Cb Y Cr (YUYV)
        break;
    case PIXFORMAT_GRAYSCALE:
        reg_write(RESET_RELATED, 0x00);  // page 0
        ret = reg_write(OUTPUT_FMT, 0xb1);  // Y only (esp32-camera value)
        break;
    default:
        return -1;  // GC0308 has no JPEG encoder
    }
    return ret;
}

static int gc0308_set_framesize(sensor_t *sensor, framesize_t framesize) {
    (void)sensor;
    int ret = 0;
    if (framesize > FRAMESIZE_VGA) {
        framesize = FRAMESIZE_VGA;
    }
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;
    uint16_t row_s = (resolution[FRAMESIZE_VGA].height - h) / 2;
    uint16_t col_s = (resolution[FRAMESIZE_VGA].width - w) / 2;

    // Subsample ratios: a smaller ratio keeps a wider view but lowers the
    // frame rate. Pick the coarsest ratio that still covers (w, h).
    struct subsample_cfg {
        uint16_t ratio_numerator;
        uint16_t ratio_denominator;
        uint8_t reg0x54;
        uint8_t reg0x56;
        uint8_t reg0x57;
        uint8_t reg0x58;
        uint8_t reg0x59;
    };
    static const struct subsample_cfg subsample_cfgs[] = {
        {84, 420, 0x55, 0x00, 0x00, 0x00, 0x00}, //1/5
        {105, 420, 0x44, 0x00, 0x00, 0x00, 0x00},//1/4
        {140, 420, 0x33, 0x00, 0x00, 0x00, 0x00},//1/3
        {210, 420, 0x22, 0x00, 0x00, 0x00, 0x00},//1/2
        {240, 420, 0x77, 0x02, 0x46, 0x02, 0x46},//4/7
        {252, 420, 0x55, 0x02, 0x04, 0x02, 0x04},//3/5
        {280, 420, 0x33, 0x02, 0x00, 0x02, 0x00},//2/3
        {420, 420, 0x11, 0x00, 0x00, 0x00, 0x00},//1/1
    };
    uint16_t win_w = resolution[FRAMESIZE_VGA].width;
    uint16_t win_h = resolution[FRAMESIZE_VGA].height;
    const struct subsample_cfg *cfg = NULL;
    // Strategy: try to keep the maximum perspective
    for (size_t i = 0; i < sizeof(subsample_cfgs) / sizeof(struct subsample_cfg); i++) {
        cfg = &subsample_cfgs[i];
        if ((win_w * cfg->ratio_numerator / cfg->ratio_denominator >= w) &&
                (win_h * cfg->ratio_numerator / cfg->ratio_denominator >= h)) {
            win_w = w * cfg->ratio_denominator / cfg->ratio_numerator;
            win_h = h * cfg->ratio_denominator / cfg->ratio_numerator;
            row_s = (resolution[FRAMESIZE_VGA].height - win_h) / 2;
            col_s = (resolution[FRAMESIZE_VGA].width - win_w) / 2;
            break;
        }
    }
    if (cfg == NULL) {
        return -1;
    }

    ret |= reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= reg_write(ROW_START_H, H8(row_s));
    ret |= reg_write(ROW_START_L, L8(row_s));
    ret |= reg_write(COL_START_H, H8(col_s));
    ret |= reg_write(COL_START_L, L8(col_s));
    ret |= reg_write(WIN_HEIGHT_H, H8(win_h + 8));
    ret |= reg_write(WIN_HEIGHT_L, L8(win_h + 8));
    ret |= reg_write(WIN_WIDTH_H, H8(win_w + 8));
    ret |= reg_write(WIN_WIDTH_L, L8(win_w + 8));

    ret |= reg_write(RESET_RELATED, 0x01);  // page 1
    ret |= set_reg_bits(SUBSAMPLE_EN, 7, 0x01, 1);
    ret |= set_reg_bits(SUBSAMPLE_EN2, 0, 0x01, 1);
    ret |= reg_write(SUBSAMPLE_MODE, cfg->reg0x54);
    ret |= reg_write(SUBSAMPLE_Y0, cfg->reg0x56);
    ret |= reg_write(SUBSAMPLE_Y1, cfg->reg0x57);
    ret |= reg_write(SUBSAMPLE_UV0, cfg->reg0x58);
    ret |= reg_write(SUBSAMPLE_UV1, cfg->reg0x59);

    ret |= reg_write(RESET_RELATED, 0x00);  // page 0
    return ret;
}

static int gc0308_set_hmirror(sensor_t *sensor, int enable) {
    (void)sensor;
    int ret = reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= set_reg_bits(CISCTL_MODE1, 0, 0x01, enable != 0);
    return ret;
}

static int gc0308_set_vflip(sensor_t *sensor, int enable) {
    (void)sensor;
    int ret = reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= set_reg_bits(CISCTL_MODE1, 1, 0x01, enable != 0);
    return ret;
}

// Masked register access (8-bit only; mask > 0xFF unsupported here)
static int gc0308_set_reg(sensor_t *sensor, int reg, int mask, int value) {
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

static int gc0308_get_reg(sensor_t *sensor, int reg, int mask) {
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

int gc0308_detect(sensor_t *sensor) {
    sensor->sccb_addr = GC0308_SCCB_ADDR;

    reg_write(RESET_RELATED, 0x00);  // page 0
    int pid = reg_read(0x00);
    if (pid != GC0308_PID) {
        // one retry, bus may still be settling
        sleep_ms(10);
        reg_write(RESET_RELATED, 0x00);
        pid = reg_read(0x00);
    }
    if (pid != GC0308_PID) {
        return -1;
    }
    sensor->id.PID = (uint16_t)pid;
    return 0;
}

int gc0308_init_sensor(sensor_t *sensor) {
    sensor->reset            = gc0308_reset;
    sensor->init_status      = NULL; // not ported yet
    sensor->set_pixformat    = gc0308_set_pixformat;
    sensor->set_framesize    = gc0308_set_framesize;
    sensor->set_brightness   = NULL; // not ported yet
    sensor->set_contrast     = NULL; // not ported yet
    sensor->set_saturation   = NULL; // not ported yet
    sensor->set_sharpness    = NULL; // not ported yet
    sensor->set_gainceiling  = NULL; // not ported yet
    sensor->set_quality      = NULL; // no JPEG, no quality setting
    sensor->set_colorbar     = NULL; // not ported yet
    sensor->set_whitebal     = NULL; // not ported yet
    sensor->set_gain_ctrl    = NULL; // not ported yet
    sensor->set_exposure_ctrl = NULL; // not ported yet
    sensor->set_hmirror      = gc0308_set_hmirror;
    sensor->set_vflip        = gc0308_set_vflip;
    sensor->set_special_effect = NULL; // not ported yet
    sensor->set_wb_mode      = NULL; // not ported yet
    sensor->set_ae_level     = NULL; // not ported yet
    sensor->set_aec_value    = NULL; // not ported yet
    sensor->set_reg          = gc0308_set_reg;
    sensor->get_reg          = gc0308_get_reg;
    sensor->set_xclk         = NULL;

    // Default register set
    regs_write(gc0308_default_init_regs);
    gc0308_apply_host_tweaks();
    sleep_ms(80);
    return 0;
}
