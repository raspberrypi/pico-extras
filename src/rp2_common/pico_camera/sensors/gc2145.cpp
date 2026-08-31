/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file gc2145.cpp
 * @brief GC2145 sensor driver.
 *
 * Register tables and control logic adapted from esp32-camera's gc2145.c
 * (Apache-2.0). The GC2145 has no JPEG encoder, so
 * set_pixformat(PIXFORMAT_JPEG) always fails; the core also rejects it
 * up front via camera_sensor_info_t.support_jpeg.
 *
 * Framesize scaling uses the subsample strategy (esp32-camera's Kconfig
 * default, CONFIG_GC_SENSOR_SUBSAMPLE_MODE): keep the widest field of
 * view and pick the coarsest subsample ratio that still covers the
 * requested window.
 */
#include "gc2145.h"

#include "pico/stdlib.h"

#include "../driver/sccb.h"
#include "gc2145_regs.h"
#include "gc2145_settings.h"

#define H8(v) ((v)>>8)
#define L8(v) ((v)&0xff)

// ---------------------------------------------------------------------------
// Low-level register access
// ---------------------------------------------------------------------------

static int reg_write(uint8_t reg, uint8_t value) {
    return sccb_write8(GC2145_SCCB_ADDR, reg, value);
}

static int reg_read(uint8_t reg) {
    uint8_t value = 0;
    if (sccb_read8(GC2145_SCCB_ADDR, reg, &value) != 0) {
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

// Host-specific tweaks, applied after the default table. Both mirror what
// esp32-camera does for slow capture hosts:
// - PLL_MODE2/CLK_DIV_MODE: divide PCLK down (esp32-camera applies this exact
//   divider on ESP32 to keep pclk <= 15 MHz). The RP2040 capture path needs
//   the headroom too: at the default PCLK, bytes are dropped and the image
//   shows per-line noise streaks (test patterns stay clean because their
//   long constant runs hide the slips). Measured on a breadboard setup with
//   XCLK = 10 MHz: fa = 2 -> 7.1 fps clean, fa = 1 -> 10.6 fps clean,
//   anything faster (f8 = 4 fa = 2, fa = 0, ...) shows sporadic or constant
//   streaks, so fa = 1 is the fastest setting that stays reliable.
// - P0_SYNC_MODE bit0: VSYNC polarity. esp32-camera sets it because the ESP32
//   camera peripheral inverts VSYNC in hardware; the PIO engine expects VSYNC
//   high during the frame, so keep it cleared (0x86 = 0x02, same value the
//   mainline Linux driver uses).
static int gc2145_apply_host_tweaks(void) {
    int ret = 0;
    ret |= reg_write(RESET_RELATED, 0x00);          // page 0
    ret |= set_reg_bits(PLL_MODE2, 0, 0x3f, 2);     // divx4
    ret |= set_reg_bits(CLK_DIV_MODE, 4, 0x0f, 1);  // divide_by
    ret |= set_reg_bits(P0_SYNC_MODE, 0, 0x01, 0);  // VSYNC high-valid
    return ret;
}

static int gc2145_reset(sensor_t *sensor) {
    (void)sensor;
    // Software reset: clear all registers back to their default values
    if (reg_write(RESET_RELATED, 0xe0) != 0) {
        return -1;
    }
    sleep_ms(100);
    if (regs_write(gc2145_default_init_regs) != 0) {
        return -1;
    }
    gc2145_apply_host_tweaks();
    sleep_ms(100);
    return 0;
}

static int gc2145_set_pixformat(sensor_t *sensor, pixformat_t pixformat) {
    (void)sensor;
    int ret;
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        reg_write(RESET_RELATED, 0x00);  // page 0
        ret = set_reg_bits(P0_OUTPUT_FORMAT, 0, 0x1f, 6);  // RGB565
        break;
    case PIXFORMAT_YUV422:
        reg_write(RESET_RELATED, 0x00);  // page 0
        ret = set_reg_bits(P0_OUTPUT_FORMAT, 0, 0x1f, 2);  // YUV422
        break;
    default:
        return -1;  // GC2145 has no JPEG encoder
    }
    return ret;
}

static int gc2145_set_framesize(sensor_t *sensor, framesize_t framesize) {
    (void)sensor;
    if (framesize > FRAMESIZE_UXGA) {
        framesize = FRAMESIZE_UXGA;
    }
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;
    uint16_t row_s = (resolution[FRAMESIZE_UXGA].height - h) / 2;
    uint16_t col_s = (resolution[FRAMESIZE_UXGA].width - w) / 2;

    // Subsample ratios: a smaller ratio keeps a wider view but lowers the
    // frame rate. Pick the coarsest ratio that still covers (w, h).
    struct subsample_cfg {
        uint16_t ratio_numerator;
        uint16_t ratio_denominator;
        uint8_t reg0x99;
        uint8_t reg0x9b;
        uint8_t reg0x9c;
        uint8_t reg0x9d;
        uint8_t reg0x9e;
        uint8_t reg0x9f;
        uint8_t reg0xa0;
        uint8_t reg0xa1;
        uint8_t reg0xa2;
    };
    static const struct subsample_cfg subsample_cfgs[] = {
        // {60, 420, 0x77, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //1/7
        // {84, 420, 0x55, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}, //1/5
        // {105, 420, 0x44, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},//1/4
        {140, 420, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},//1/3
        {210, 420, 0x22, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},//1/2
        {240, 420, 0x77, 0x02, 0x46, 0x02, 0x46, 0x02, 0x46, 0x02, 0x46},//4/7
        {252, 420, 0x55, 0x02, 0x04, 0x02, 0x04, 0x02, 0x04, 0x02, 0x04},//3/5
        {280, 420, 0x33, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00},//2/3
        {420, 420, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},//1/1
    };
    uint16_t win_w = resolution[FRAMESIZE_UXGA].width;
    uint16_t win_h = resolution[FRAMESIZE_UXGA].height;
    const struct subsample_cfg *cfg = NULL;
    // Strategy: try to keep the maximum perspective
    size_t i = 0;
    if (framesize >= FRAMESIZE_QVGA) {
        i = 1;
    }
    for (; i < sizeof(subsample_cfgs) / sizeof(struct subsample_cfg); i++) {
        cfg = &subsample_cfgs[i];
        if ((win_w * cfg->ratio_numerator / cfg->ratio_denominator >= w) &&
                (win_h * cfg->ratio_numerator / cfg->ratio_denominator >= h)) {
            win_w = w * cfg->ratio_denominator / cfg->ratio_numerator;
            win_h = h * cfg->ratio_denominator / cfg->ratio_numerator;
            row_s = (resolution[FRAMESIZE_UXGA].height - win_h) / 2;
            col_s = (resolution[FRAMESIZE_UXGA].width - win_w) / 2;
            break;
        }
    }
    if (cfg == NULL) {
        return -1;
    }

    reg_write(RESET_RELATED, 0x00);  // page 0
    reg_write(P0_CROP_ENABLE, 0x01);
    reg_write(0x09, H8(row_s));
    reg_write(0x0a, L8(row_s));
    reg_write(0x0b, H8(col_s));
    reg_write(0x0c, L8(col_s));
    reg_write(0x0d, H8(win_h + 8));
    reg_write(0x0e, L8(win_h + 8));
    reg_write(0x0f, H8(win_w + 16));
    reg_write(0x10, L8(win_w + 16));

    reg_write(0x99, cfg->reg0x99);
    reg_write(0x9b, cfg->reg0x9b);
    reg_write(0x9c, cfg->reg0x9c);
    reg_write(0x9d, cfg->reg0x9d);
    reg_write(0x9e, cfg->reg0x9e);
    reg_write(0x9f, cfg->reg0x9f);
    reg_write(0xa0, cfg->reg0xa0);
    reg_write(0xa1, cfg->reg0xa1);
    reg_write(0xa2, cfg->reg0xa2);

    reg_write(0x95, H8(h));
    reg_write(0x96, L8(h));
    reg_write(0x97, H8(w));
    reg_write(0x98, L8(w));
    return 0;
}

static int gc2145_set_hmirror(sensor_t *sensor, int enable) {
    (void)sensor;
    int ret = reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= set_reg_bits(P0_ANALOG_MODE1, 0, 0x01, enable != 0);
    return ret;
}

static int gc2145_set_vflip(sensor_t *sensor, int enable) {
    (void)sensor;
    int ret = reg_write(RESET_RELATED, 0x00);  // page 0
    ret |= set_reg_bits(P0_ANALOG_MODE1, 1, 0x01, enable != 0);
    return ret;
}

// Masked register access (8-bit only; mask > 0xFF unsupported here)
static int gc2145_set_reg(sensor_t *sensor, int reg, int mask, int value) {
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

static int gc2145_get_reg(sensor_t *sensor, int reg, int mask) {
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

int gc2145_detect(sensor_t *sensor) {
    sensor->sccb_addr = GC2145_SCCB_ADDR;

    int midh = reg_read(CHIP_ID_HIGH);
    int midl = reg_read(CHIP_ID_LOW);
    uint16_t pid = (uint16_t)(midh << 8 | midl);
    if (pid != GC2145_PID) {
        // one retry, bus may still be settling
        sleep_ms(10);
        midh = reg_read(CHIP_ID_HIGH);
        midl = reg_read(CHIP_ID_LOW);
        pid = (uint16_t)(midh << 8 | midl);
    }
    if (pid != GC2145_PID) {
        return -1;
    }
    sensor->id.PID = pid;
    return 0;
}

int gc2145_init_sensor(sensor_t *sensor) {
    sensor->reset            = gc2145_reset;
    sensor->init_status      = NULL; // not ported yet
    sensor->set_pixformat    = gc2145_set_pixformat;
    sensor->set_framesize    = gc2145_set_framesize;
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
    sensor->set_hmirror      = gc2145_set_hmirror;
    sensor->set_vflip        = gc2145_set_vflip;
    sensor->set_special_effect = NULL; // not ported yet
    sensor->set_wb_mode      = NULL; // not ported yet
    sensor->set_ae_level     = NULL; // not ported yet
    sensor->set_aec_value    = NULL; // not ported yet
    sensor->set_reg          = gc2145_set_reg;
    sensor->get_reg          = gc2145_get_reg;
    sensor->set_xclk         = NULL;

    // Default register set
    regs_write(gc2145_default_init_regs);
    gc2145_apply_host_tweaks();
    sleep_ms(100);
    return 0;
}
