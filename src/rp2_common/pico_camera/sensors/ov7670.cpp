/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file ov7670.cpp
 * @brief OV7670 sensor driver.
 *
 * Register tables and control logic adapted from esp32-camera's ov7670.c
 * (OpenMV, MIT license). The OV7670 has no JPEG encoder, so
 * set_pixformat(PIXFORMAT_JPEG) always fails; the core also rejects it
 * up front via camera_sensor_info_t.support_jpeg.
 *
 * Timing notes:
 * - The default table targets "30 fps @ 12 MHz XCLK" (CLKRC=0, PLL x4).
 *   With this library's 10 MHz XCLK the frame rate scales to ~25 fps.
 * - COM10 selects VSYNC active during frame data (VSYNC_NEG) and a
 *   free-running PCLK, matching what the PIO capture engine expects.
 */
#include "ov7670.h"

#include "pico/stdlib.h"

#include "../driver/sccb.h"
#include "ov7670_regs.h"

// ---------------------------------------------------------------------------
// Register tables (from Omnivision via esp32-camera / linux-ov7670)
// ---------------------------------------------------------------------------

// Default settings, giving VGA YUYV. There is really no making sense of
// most of these - lots of "reserved" values from Omnivision.
static const uint8_t ov7670_default_regs[][2] = {
    /* Sensor automatically sets output window when resolution changes. */
    {TSLB, 0x04},

    /* Frame rate 30 fps at 12 MHz clock */
    {CLKRC, 0x00},
    {DBLV,  0x4A},

    {COM10, COM10_VSYNC_NEG | COM10_PCLK_FREE},

    /* Improve white balance */
    {COM4, 0x40},

    /* Improve color */
    {RSVD_B0, 0x84},

    /* Enable 50/60 Hz auto detection */
    {COM11, COM11_EXP | COM11_HZAUTO},

    /* Disable some delays */
    {HSYST, 0},
    {HSYEN, 0},

    {MVFP, MVFP_SUN},

    /* More reserved magic, some of which tweaks white balance */
    {AWBC1, 0x0a},
    {AWBC2, 0xf0},
    {AWBC3, 0x34},
    {AWBC4, 0x58},
    {AWBC5, 0x28},
    {AWBC6, 0x3a},

    {AWBCTR3, 0x0a},
    {AWBCTR2, 0x55},
    {AWBCTR1, 0x11},
    {AWBCTR0, 0x9e},

    {COM8, COM8_FAST_AUTO | COM8_STEP_UNLIMIT | COM8_AGC_EN | COM8_AEC_EN | COM8_AWB_EN},

    {0xFF, 0xFF},  // end marker
};

static const uint8_t ov7670_fmt_yuv422[][2] = {
    {COM7,     0x0                       },  /* Selects YUV mode */
    {RGB444,   0                         },  /* No RGB444 please */
    {COM1,     0                         },  /* CCIR601 */
    {COM15,    COM15_R00FF               },
    {MVFP,     MVFP_SUN                  },
    {COM9,     0x6A                      },  /* 128x gain ceiling; 0x8 is reserved bit */
    {MTX1,     0x80                      },  /* "matrix coefficient 1" */
    {MTX2,     0x80                      },  /* "matrix coefficient 2" */
    {MTX3,     0                         },  /* vb */
    {MTX4,     0x22                      },  /* "matrix coefficient 4" */
    {MTX5,     0x5e                      },  /* "matrix coefficient 5" */
    {MTX6,     0x80                      },  /* "matrix coefficient 6" */
    {COM13,    COM13_UVSAT | COM13_UVSWAP},  /* UV swap: sensor emits YVYU, we stream YUYV */
    {0xff,     0xff                      },  // end marker
};

static const uint8_t ov7670_fmt_rgb565[][2] = {
    {COM7,     COM7_FMT_RGB565           },  /* Selects RGB mode */
    {RGB444,   0                         },  /* No RGB444 please */
    {COM1,     0x0                       },  /* CCIR601 */
    {COM15,    COM15_RGB565 | COM15_R00FF},
    {MVFP,     MVFP_SUN                  },
    {COM9,     0x6A                      },  /* 128x gain ceiling; 0x8 is reserved bit */
    {MTX1,     0xb3                      },  /* "matrix coefficient 1" */
    {MTX2,     0xb3                      },  /* "matrix coefficient 2" */
    {MTX3,     0                         },  /* vb */
    {MTX4,     0x3d                      },  /* "matrix coefficient 4" */
    {MTX5,     0xa7                      },  /* "matrix coefficient 5" */
    {MTX6,     0xe4                      },  /* "matrix coefficient 6" */
    {COM13,    COM13_UVSAT               },
    {0xff,     0xff                      },  // end marker
};

static const uint8_t ov7670_vga[][2] = {
    {COM3,                 0x00},
    {COM14,                0x00},
    {SCALING_XSC,          0x3A},
    {SCALING_YSC,          0x35},
    {SCALING_DCWCTR,       0x11},
    {SCALING_PCLK_DIV,     0xF0},
    {SCALING_PCLK_DELAY,   0x02},
    {0xff, 0xff},
};

static const uint8_t ov7670_qvga[][2] = {
    {COM3,                 0x04},
    {COM14,                0x19},
    {SCALING_XSC,          0x3A},
    {SCALING_YSC,          0x35},
    {SCALING_DCWCTR,       0x11},
    {SCALING_PCLK_DIV,     0xF1},
    {SCALING_PCLK_DELAY,   0x02},
    {0xff, 0xff},
};

static const uint8_t ov7670_qqvga[][2] = {
    {COM3,                 0x04},  // DCW enable
    {COM14,                0x1a},  // PCLK divided by 4, manual scaling, DCW+PCLK register controlled
    {SCALING_XSC,          0x3a},
    {SCALING_YSC,          0x35},
    {SCALING_DCWCTR,       0x22},  // downsample by 4
    {SCALING_PCLK_DIV,     0xf2},  // PCLK divided by 4
    {SCALING_PCLK_DELAY,   0x02},
    {0xff, 0xff},
};

// ---------------------------------------------------------------------------
// Low-level register access
// ---------------------------------------------------------------------------

static int reg_write(uint8_t reg, uint8_t value) {
    return sccb_write8(OV7670_SCCB_ADDR, reg, value);
}

static uint8_t reg_read(uint8_t reg) {
    uint8_t value = 0;
    sccb_read8(OV7670_SCCB_ADDR, reg, &value);
    return value;
}

static void regs_write(const uint8_t (*regs)[2]) {
    sccb_write_list8(OV7670_SCCB_ADDR, regs);
}

// CLKRC survives framesize changes; rewriting it after selecting RGB565
// avoids a washed-out image (esp32-camera comment: RGB565 needs clkrc
// rewritten after the other parameters or the image looks poor).
static uint8_t s_clkrc = 0x01;

// Horizontal/vertical window: hstart/hstop columns, vstart/vstop rows.
// Values below are the Omnivision recommended ones for full-frame output.
static int frame_control(int hstart, int hstop, int vstart, int vstop) {
    uint8_t frame[][2] = {
        {HSTART, (uint8_t)(hstart >> 3)},
        {HSTOP, (uint8_t)(hstop >> 3)},
        {HREF, (uint8_t)(((hstop & 0x07) << 3) | (hstart & 0x07))},
        {VSTART, (uint8_t)(vstart >> 2)},
        {VSTOP, (uint8_t)(vstop >> 2)},
        {VREF, (uint8_t)(((vstop & 0x02) << 2) | (vstart & 0x02))},
        {0xFF,   0xFF},
    };
    regs_write(frame);
    return 0;
}

// ---------------------------------------------------------------------------
// sensor_t interface
// ---------------------------------------------------------------------------

static int ov7670_reset(sensor_t *sensor) {
    (void)sensor;
    reg_write(COM7, COM7_RESET);  // reset all registers to defaults
    sleep_ms(10);
    regs_write(ov7670_default_regs);
    sleep_ms(30);
    return 0;
}

static int ov7670_set_pixformat(sensor_t *sensor, pixformat_t pixformat) {
    (void)sensor;
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        regs_write(ov7670_fmt_rgb565);
        sleep_ms(30);
        // RGB565 requires clkrc to be rewritten after the other parameters
        reg_write(CLKRC, s_clkrc);
        return 0;
    case PIXFORMAT_YUV422:
        regs_write(ov7670_fmt_yuv422);
        sleep_ms(30);
        // ...but in YUV mode clkrc must NOT be rewritten (image degrades)
        return 0;
    default:
        return -1;  // OV7670 has no JPEG encoder
    }
}

static int ov7670_set_framesize(sensor_t *sensor, framesize_t framesize) {
    (void)sensor;
    int ret = -1;

    // Store clkrc before changing window settings
    s_clkrc = reg_read(CLKRC);

    switch (framesize) {
    case FRAMESIZE_VGA:
        regs_write(ov7670_vga);
        ret = frame_control(158, 14, 10, 490);  // values from Omnivision
        break;
    case FRAMESIZE_QVGA:
        regs_write(ov7670_qvga);
        ret = frame_control(158, 14, 10, 490);
        break;
    case FRAMESIZE_QQVGA:
        regs_write(ov7670_qqvga);
        ret = frame_control(158, 14, 12, 490);
        break;
    default:
        break;
    }

    sleep_ms(30);
    return ret;
}

static int ov7670_set_colorbar(sensor_t *sensor, int enable) {
    (void)sensor;
    // SCALING_XSC bit7 must stay 0, SCALING_YSC bit7 toggles the color bar
    uint8_t reg = SCALING_XSC_CBAR(reg_read(SCALING_XSC));
    int ret = reg_write(SCALING_XSC, reg);
    reg = SCALING_YSC_CBAR(reg_read(SCALING_YSC), enable);
    ret |= reg_write(SCALING_YSC, reg);
    return ret;
}

static int ov7670_set_whitebal(sensor_t *sensor, int enable) {
    (void)sensor;
    return reg_write(COM8, COM8_SET_AWB(reg_read(COM8), enable));
}

static int ov7670_set_gain_ctrl(sensor_t *sensor, int enable) {
    (void)sensor;
    return reg_write(COM8, COM8_SET_AGC(reg_read(COM8), enable));
}

static int ov7670_set_exposure_ctrl(sensor_t *sensor, int enable) {
    (void)sensor;
    return reg_write(COM8, COM8_SET_AEC(reg_read(COM8), enable));
}

static int ov7670_set_hmirror(sensor_t *sensor, int enable) {
    (void)sensor;
    return reg_write(MVFP, MVFP_SET_MIRROR(reg_read(MVFP), enable));
}

static int ov7670_set_vflip(sensor_t *sensor, int enable) {
    (void)sensor;
    return reg_write(MVFP, MVFP_SET_FLIP(reg_read(MVFP), enable));
}

// Masked register access (8-bit only; mask > 0xFF unsupported here)
static int ov7670_set_reg(sensor_t *sensor, int reg, int mask, int value) {
    (void)sensor;
    uint8_t old = reg_read((uint8_t)reg);
    value = (old & ~mask) | (value & mask);
    reg_write((uint8_t)reg, (uint8_t)value);
    return value;
}

static int ov7670_get_reg(sensor_t *sensor, int reg, int mask) {
    (void)sensor;
    return reg_read((uint8_t)reg) & mask;
}

// ---------------------------------------------------------------------------
// Detection / registration
// ---------------------------------------------------------------------------

int ov7670_detect(sensor_t *sensor) {
    sensor->sccb_addr = OV7670_SCCB_ADDR;

    uint8_t pid = reg_read(REG_PID);
    if (pid != OV7670_PID) {
        // one retry, bus may still be settling
        sleep_ms(10);
        pid = reg_read(REG_PID);
    }
    if (pid != OV7670_PID) {
        return -1;
    }
    sensor->id.PID = pid;
    sensor->id.VER = reg_read(REG_VER);
    sensor->id.MIDH = reg_read(REG_MIDH);
    sensor->id.MIDL = reg_read(REG_MIDL);
    return 0;
}

int ov7670_init_sensor(sensor_t *sensor) {
    sensor->reset            = ov7670_reset;
    sensor->init_status      = NULL; // not ported yet
    sensor->set_pixformat    = ov7670_set_pixformat;
    sensor->set_framesize    = ov7670_set_framesize;
    sensor->set_brightness   = NULL; // not ported yet
    sensor->set_contrast     = NULL; // not ported yet
    sensor->set_saturation   = NULL; // not ported yet
    sensor->set_sharpness    = NULL; // not ported yet
    sensor->set_gainceiling  = NULL; // not ported yet
    sensor->set_quality      = NULL; // no JPEG, no quality setting
    sensor->set_colorbar     = ov7670_set_colorbar;
    sensor->set_whitebal     = ov7670_set_whitebal;
    sensor->set_gain_ctrl    = ov7670_set_gain_ctrl;
    sensor->set_exposure_ctrl = ov7670_set_exposure_ctrl;
    sensor->set_hmirror      = ov7670_set_hmirror;
    sensor->set_vflip        = ov7670_set_vflip;
    sensor->set_special_effect = NULL; // not ported yet
    sensor->set_wb_mode      = NULL; // not ported yet
    sensor->set_ae_level     = NULL; // not ported yet
    sensor->set_aec_value    = NULL; // not ported yet
    sensor->set_reg          = ov7670_set_reg;
    sensor->get_reg          = ov7670_get_reg;
    sensor->set_xclk         = NULL;

    // Default register set
    regs_write(ov7670_default_regs);
    sleep_ms(30);
    return 0;
}
