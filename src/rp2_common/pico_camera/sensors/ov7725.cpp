/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file ov7725.cpp
 * @brief OV7725 sensor driver.
 *
 * Register tables and control logic adapted from esp32-camera's ov7725.c
 * (OpenMV, MIT license). The OV7725 has no JPEG encoder, so
 * set_pixformat(PIXFORMAT_JPEG) always fails; the core also rejects it
 * up front via camera_sensor_info_t.support_jpeg.
 *
 * Timing notes (same register family as the OV7670):
 * - COM10 selects VSYNC active during frame data (VSYNC_NEG) and a
 *   free-running PCLK, matching what the PIO capture engine expects.
 * - The default table bypasses the PLL and divides PCLK down via CLKRC
 *   (QVGA: XCLK/4, VGA: XCLK/8), which the RP2040 capture path needs.
 */
#include "ov7725.h"

#include "pico/stdlib.h"

#include "../driver/sccb.h"
#include "ov7725_regs.h"

// ---------------------------------------------------------------------------
// Register tables (from Omnivision via esp32-camera / OpenMV)
// ---------------------------------------------------------------------------

static const uint8_t ov7725_default_regs[][2] = {
    {COM3,          COM3_SWAP_YUV},
    {COM7,          COM7_RES_QVGA | COM7_FMT_YUV},

    {COM4,          0x01 | 0x00}, /* bypass PLL (0x00:off, 0x40:4x, 0x80:6x, 0xC0:8x) */
    {CLKRC,         0x80 | 0x03}, /* Res/Bypass pre-scalar (0x40:bypass, 0x00-0x3F:prescaler PCLK=XCLK/(prescaler + 1)/2 ) */

    // QVGA Window Size
    {HSTART,        0x3F},
    {HSIZE,         0x50},
    {VSTART,        0x03},
    {VSIZE,         0x78},
    {HREF,          0x00},

    // Scale down to QVGA Resolution
    {HOUTSIZE,      0x50},
    {VOUTSIZE,      0x78},
    {EXHCH,         0x00},

    {COM12,         0x03},
    {TGT_B,         0x7F},
    {FIXGAIN,       0x09},
    {AWB_CTRL0,     0xE0},
    {DSP_CTRL1,     0xFF},

    {DSP_CTRL2,     DSP_CTRL2_VDCW_EN | DSP_CTRL2_HDCW_EN | DSP_CTRL2_HZOOM_EN | DSP_CTRL2_VZOOM_EN},

    {DSP_CTRL3,     0x00},
    {DSP_CTRL4,     0x00},
    {DSPAUTO,       0xFF},

    {COM8,          0xF0},
    {COM6,          0xC5},
    {COM9,          0x11},
    {COM10,         COM10_VSYNC_NEG | COM10_PCLK_FREE}, // VSYNC high during frame + free-running PCLK
    {BDBASE,        0x7F},
    {DBSTEP,        0x03},
    {AEW,           0x75},
    {AEB,           0x64},
    {VPT,           0xA1},
    {EXHCL,         0x00},
    {AWB_CTRL3,     0xAA},
    {COM8,          0xFF},

    //Gamma
    {GAM1,          0x0C},
    {GAM2,          0x16},
    {GAM3,          0x2A},
    {GAM4,          0x4E},
    {GAM5,          0x61},
    {GAM6,          0x6F},
    {GAM7,          0x7B},
    {GAM8,          0x86},
    {GAM9,          0x8E},
    {GAM10,         0x97},
    {GAM11,         0xA4},
    {GAM12,         0xAF},
    {GAM13,         0xC5},
    {GAM14,         0xD7},
    {GAM15,         0xE8},

    {SLOP,          0x20},
    {EDGE1,         0x05},
    {EDGE2,         0x03},
    {EDGE3,         0x00},
    {DNSOFF,        0x01},

    {MTX1,          0xB0},
    {MTX2,          0x9D},
    {MTX3,          0x13},
    {MTX4,          0x16},
    {MTX5,          0x7B},
    {MTX6,          0x91},
    {MTX_CTRL,      0x1E},

    {BRIGHTNESS,    0x08},
    {CONTRAST,      0x30},
    {UVADJ0,        0x81},
    {SDE, (SDE_CONT_BRIGHT_EN | SDE_SATURATION_EN)},

    // For 30 fps/60Hz
    {DM_LNL,        0x00},
    {DM_LNH,        0x00},
    {BDBASE,        0x7F},
    {DBSTEP,        0x03},

    // Lens Correction, should be tuned with real camera module
    {LC_RADI,       0x10},
    {LC_COEF,       0x10},
    {LC_COEFB,      0x14},
    {LC_COEFR,      0x17},
    {LC_CTR,        0x05},
    {COM5,          0xF5}, //0x65

    {0xFF,          0xFF},  // end marker
};

// ---------------------------------------------------------------------------
// Low-level register access
// ---------------------------------------------------------------------------

static int reg_write(uint8_t reg, uint8_t value) {
    return sccb_write8(OV7725_SCCB_ADDR, reg, value);
}

static int reg_read(uint8_t reg) {
    uint8_t value = 0;
    if (sccb_read8(OV7725_SCCB_ADDR, reg, &value) != 0) {
        return -1;
    }
    return value;
}

static void regs_write(const uint8_t (*regs)[2]) {
    sccb_write_list8(OV7725_SCCB_ADDR, regs);
}

// esp32-camera semantics: offset + bit count (not a mask)
static int set_reg_bits(uint8_t reg, uint8_t offset, uint8_t length, uint8_t value) {
    int old = reg_read(reg);
    if (old < 0) {
        return old;
    }
    uint8_t mask = (uint8_t)(((1 << length) - 1) << offset);
    uint8_t new_value = (uint8_t)((old & ~mask) | ((value << offset) & mask));
    return reg_write(reg, new_value);
}

// ---------------------------------------------------------------------------
// sensor_t interface
// ---------------------------------------------------------------------------

static int ov7725_reset(sensor_t *sensor) {
    (void)sensor;
    reg_write(COM7, COM7_RESET);  // reset all registers to defaults
    sleep_ms(10);
    regs_write(ov7725_default_regs);
    sleep_ms(30);
    return 0;
}

static int ov7725_set_pixformat(sensor_t *sensor, pixformat_t pixformat) {
    (void)sensor;
    if (pixformat != PIXFORMAT_RGB565) {
        return -1;  // OV7725 has no JPEG encoder
    }
    int reg = reg_read(COM7);
    if (reg < 0) {
        return reg;
    }
    int ret = reg_write(COM7, (uint8_t)COM7_SET_RGB(reg, COM7_FMT_RGB565));
    sleep_ms(30);
    return ret;
}

static int ov7725_set_framesize(sensor_t *sensor, framesize_t framesize) {
    (void)sensor;
    int ret = 0;
    if (framesize > FRAMESIZE_VGA) {
        return -1;
    }
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;
    int reg = reg_read(COM7);
    if (reg < 0) {
        return reg;
    }

    // Write MSBs
    ret |= reg_write(HOUTSIZE, (uint8_t)(w >> 2));
    ret |= reg_write(VOUTSIZE, (uint8_t)(h >> 1));

    ret |= reg_write(HSIZE, (uint8_t)(w >> 2));
    ret |= reg_write(VSIZE, (uint8_t)(h >> 1));

    // Write LSBs
    ret |= reg_write(HREF, (uint8_t)((w & 0x3) | ((h & 0x1) << 2)));

    if (framesize < FRAMESIZE_VGA) {
        // Enable auto-scaling/zooming factors
        ret |= reg_write(DSPAUTO, 0xFF);

        ret |= reg_write(HSTART, 0x3F);
        ret |= reg_write(VSTART, 0x03);

        ret |= reg_write(COM7, (uint8_t)(reg | COM7_RES_QVGA));

        ret |= reg_write(CLKRC, 0x80 | 0x01);
    } else {
        // Disable auto-scaling/zooming factors
        ret |= reg_write(DSPAUTO, 0xF3);

        // Clear auto-scaling/zooming factors
        ret |= reg_write(SCAL0, 0x00);
        ret |= reg_write(SCAL1, 0x00);
        ret |= reg_write(SCAL2, 0x00);

        ret |= reg_write(HSTART, 0x23);
        ret |= reg_write(VSTART, 0x07);

        ret |= reg_write(COM7, (uint8_t)(reg & ~COM7_RES_QVGA));

        ret |= reg_write(CLKRC, 0x80 | 0x03);
    }

    sleep_ms(30);
    return ret;
}

static int ov7725_set_colorbar(sensor_t *sensor, int enable) {
    (void)sensor;
    // Both the sensor pattern (COM3) and the DSP path (DSP_CTRL3) bits
    int reg = reg_read(COM3);
    if (reg < 0) {
        return reg;
    }
    int ret = reg_write(COM3, (uint8_t)COM3_SET_CBAR(reg, enable));
    reg = reg_read(DSP_CTRL3);
    if (reg < 0) {
        return reg;
    }
    ret |= reg_write(DSP_CTRL3, (uint8_t)DSP_CTRL3_SET_CBAR(reg, enable));
    return ret;
}

static int ov7725_set_whitebal(sensor_t *sensor, int enable) {
    (void)sensor;
    return set_reg_bits(COM8, 1, 1, enable != 0);
}

static int ov7725_set_gain_ctrl(sensor_t *sensor, int enable) {
    (void)sensor;
    return set_reg_bits(COM8, 2, 1, enable != 0);
}

static int ov7725_set_exposure_ctrl(sensor_t *sensor, int enable) {
    (void)sensor;
    return set_reg_bits(COM8, 0, 1, enable != 0);
}

static int ov7725_set_hmirror(sensor_t *sensor, int enable) {
    (void)sensor;
    return set_reg_bits(COM3, 6, 1, enable != 0);
}

static int ov7725_set_vflip(sensor_t *sensor, int enable) {
    (void)sensor;
    return set_reg_bits(COM3, 7, 1, enable != 0);
}

static int ov7725_set_brightness(sensor_t *sensor, int level) {
    (void)sensor;
    return reg_write(BRIGHTNESS, (uint8_t)level);
}

static int ov7725_set_contrast(sensor_t *sensor, int level) {
    (void)sensor;
    return reg_write(CONTRAST, (uint8_t)level);
}

static int ov7725_set_aec_value(sensor_t *sensor, int value) {
    (void)sensor;
    return reg_write(AEC, (uint8_t)(value & 0xff)) | reg_write(AECH, (uint8_t)(value >> 8));
}

// Masked register access (8-bit only; mask > 0xFF unsupported here)
static int ov7725_set_reg(sensor_t *sensor, int reg, int mask, int value) {
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

static int ov7725_get_reg(sensor_t *sensor, int reg, int mask) {
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

int ov7725_detect(sensor_t *sensor) {
    sensor->sccb_addr = OV7725_SCCB_ADDR;

    reg_write(0xFF, 0x01);  // bank: sensor
    int pid = reg_read(REG_PID);
    if (pid != OV7725_PID) {
        // one retry, bus may still be settling
        sleep_ms(10);
        reg_write(0xFF, 0x01);
        pid = reg_read(REG_PID);
    }
    if (pid != OV7725_PID) {
        return -1;
    }
    sensor->id.PID = (uint16_t)pid;
    sensor->id.VER = (uint8_t)reg_read(REG_VER);
    sensor->id.MIDH = (uint8_t)reg_read(REG_MIDH);
    sensor->id.MIDL = (uint8_t)reg_read(REG_MIDL);
    return 0;
}

int ov7725_init_sensor(sensor_t *sensor) {
    sensor->reset            = ov7725_reset;
    sensor->init_status      = NULL; // not ported yet
    sensor->set_pixformat    = ov7725_set_pixformat;
    sensor->set_framesize    = ov7725_set_framesize;
    sensor->set_brightness   = ov7725_set_brightness;
    sensor->set_contrast     = ov7725_set_contrast;
    sensor->set_saturation   = NULL; // not supported by the sensor via this API
    sensor->set_sharpness    = NULL; // not supported by the sensor via this API
    sensor->set_gainceiling  = NULL; // not ported yet
    sensor->set_quality      = NULL; // no JPEG, no quality setting
    sensor->set_colorbar     = ov7725_set_colorbar;
    sensor->set_whitebal     = ov7725_set_whitebal;
    sensor->set_gain_ctrl    = ov7725_set_gain_ctrl;
    sensor->set_exposure_ctrl = ov7725_set_exposure_ctrl;
    sensor->set_hmirror      = ov7725_set_hmirror;
    sensor->set_vflip        = ov7725_set_vflip;
    sensor->set_special_effect = NULL; // not ported yet
    sensor->set_wb_mode      = NULL; // not ported yet
    sensor->set_ae_level     = NULL; // not ported yet
    sensor->set_aec_value    = ov7725_set_aec_value;
    sensor->set_reg          = ov7725_set_reg;
    sensor->get_reg          = ov7725_get_reg;
    sensor->set_xclk         = NULL;

    // Default register set
    regs_write(ov7725_default_regs);
    sleep_ms(30);
    return 0;
}
