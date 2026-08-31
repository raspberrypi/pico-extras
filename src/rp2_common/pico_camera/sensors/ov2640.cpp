/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ov2640.h"

#include "pico/stdlib.h"

#include "../driver/sccb.h"
#include "ov2640_regs.h"
#include "ov2640_settings.h"

// ---------------------------------------------------------------------------
// Internal state (single sensor instance is assumed)
// ---------------------------------------------------------------------------

static pixformat_t s_pixformat = PIXFORMAT_RGB565;
static framesize_t s_framesize = FRAMESIZE_QVGA;

typedef enum {
    OV2640_MODE_UXGA, OV2640_MODE_SVGA, OV2640_MODE_CIF
} ov2640_sensor_mode_t;

typedef struct {
    uint16_t offset_x;
    uint16_t offset_y;
    uint16_t max_x;
    uint16_t max_y;
} ratio_settings_t;

static const ratio_settings_t ratio_table[] = {
    {   0,   0, 1600, 1200 }, // 4x3
    {   8,  72, 1584, 1056 }, // 3x2
    {   0, 100, 1600, 1000 }, // 16x10
    {   0, 120, 1600,  960 }, // 5x3
    {   0, 150, 1600,  900 }, // 16x9
    {   2, 258, 1596,  684 }, // 21x9
    {  50,   0, 1500, 1200 }, // 5x4
    { 200,   0, 1200, 1200 }, // 1x1
    { 462,   0,  676, 1200 }  // 9x16
};

// ---------------------------------------------------------------------------
// Low-level register access
// ---------------------------------------------------------------------------

static void reg_write(uint8_t reg, uint8_t value) {
    sccb_write8(OV2640_SCCB_ADDR, reg, value);
}

static uint8_t reg_read(uint8_t reg) {
    uint8_t value = 0;
    sccb_read8(OV2640_SCCB_ADDR, reg, &value);
    return value;
}

static void regs_write(const uint8_t (*regs)[2]) {
    sccb_write_list8(OV2640_SCCB_ADDR, regs);
}

// Read-modify-write of a bit field within a register of the given bank
static void set_reg_bits(uint8_t bank, uint8_t reg, uint8_t offset, uint8_t mask, uint8_t value) {
    reg_write(BANK_SEL, bank);
    uint8_t v = reg_read(reg);
    v = (uint8_t)((v & ~(mask << offset)) | ((value & mask) << offset));
    reg_write(reg, v);
}

static void sreset(void) {
    reg_write(BANK_SEL, BANK_SEL_SENSOR);
    reg_write(COM7, COM7_SRST);
    sleep_ms(10);
}

static uint16_t read_pid(void) {
    reg_write(BANK_SEL, BANK_SEL_SENSOR);
    sleep_ms(5);
    uint8_t pidh = reg_read(0x0A);
    sleep_ms(1);
    uint8_t pidl = reg_read(0x0B);
    return ((uint16_t)pidh << 8) | pidl;
}

// ---------------------------------------------------------------------------
// sensor_t interface
// ---------------------------------------------------------------------------

static int ov2640_reset(sensor_t *sensor) {
    (void)sensor;
    sreset();
    sleep_ms(200);
    regs_write(ov2640_settings_cif);
    sleep_ms(50);
    return 0;
}

static int ov2640_set_pixformat(sensor_t *sensor, pixformat_t pixformat) {
    (void)sensor;
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        regs_write(ov2640_settings_rgb565);
        break;
    case PIXFORMAT_YUV422:
        regs_write(ov2640_settings_yuv422);
        break;
    case PIXFORMAT_JPEG:
        regs_write(ov2640_settings_jpeg);
        break;
    default:
        return -1;
    }
    s_pixformat = pixformat;
    sleep_ms(15);
    return 0;
}

#define VAL_SET(x, mask, rshift, lshift) ((((x) >> (rshift)) & mask) << (lshift))

static int ov2640_set_framesize(sensor_t *sensor, framesize_t framesize) {
    (void)sensor;
    if (framesize >= FRAMESIZE_INVALID) {
        return -1;
    }

    const uint8_t (*regs)[2];
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;
    aspect_ratio_t ratio = resolution[framesize].aspect_ratio;
    uint16_t max_x = ratio_table[ratio].max_x;
    uint16_t max_y = ratio_table[ratio].max_y;
    uint16_t offset_x = ratio_table[ratio].offset_x;
    uint16_t offset_y = ratio_table[ratio].offset_y;
    ov2640_sensor_mode_t mode = OV2640_MODE_UXGA;
    uint8_t clkrc, pclk_div;

    if (framesize <= FRAMESIZE_CIF) {
        mode = OV2640_MODE_CIF;
        max_x /= 4;
        max_y /= 4;
        offset_x /= 4;
        offset_y /= 4;
        if (max_y > 296) {
            max_y = 296;
        }
    } else if (framesize <= FRAMESIZE_SVGA) {
        mode = OV2640_MODE_SVGA;
        max_x /= 2;
        max_y /= 2;
        offset_x /= 2;
        offset_y /= 2;
    }

    uint8_t size_regs[][2] = {
        { BANK_SEL, BANK_SEL_DSP },
        { RESET, RESET_DVP },
        { HSIZE, (uint8_t)((max_x >> 2) & 0xFF) },
        { VSIZE, (uint8_t)((max_y >> 2) & 0xFF) },
        { XOFFL, (uint8_t)(offset_x & 0xFF) },
        { YOFFL, (uint8_t)(offset_y & 0xFF) },
        {
            VHYX, (uint8_t)(VAL_SET(max_y, 0x1, 10, 7) |
                            VAL_SET(offset_y, 0x7, 8, 4) |
                            VAL_SET(max_x, 0x1, 10, 3) |
                            VAL_SET(offset_x, 0x7, 8, 0))
        },
        { TEST, (uint8_t)((max_x >> 11) << 7) },
        { ZMOW, (uint8_t)((w >> 2) & 0xFF) },
        { ZMOH, (uint8_t)((h >> 2) & 0xFF) },
        { ZMHH, (uint8_t)((((h >> 10) & 0x1) << 2) | ((w >> 10) & 0x3)) },
        ENDMARKER,
    };

    if (s_pixformat == PIXFORMAT_JPEG) {
        clkrc = 0x00;
        pclk_div = 8;
        if (mode == OV2640_MODE_UXGA) {
            pclk_div = 12;
        }
    } else {
        pclk_div = R_DVP_SP_AUTO_MODE;
        clkrc = CLKRC_2X | CLKRC_DIV(8);
        if (mode == OV2640_MODE_CIF) {
            clkrc = CLKRC_2X | CLKRC_DIV(2);
        } else if (mode == OV2640_MODE_UXGA) {
            pclk_div = 12;
        }
    }

    if (mode == OV2640_MODE_CIF) {
        regs = ov2640_settings_to_cif;
    } else if (mode == OV2640_MODE_SVGA) {
        regs = ov2640_settings_to_svga;
    } else {
        regs = ov2640_settings_to_uxga;
    }

    // Bypass the DSP while changing settings
    reg_write(BANK_SEL, BANK_SEL_DSP);
    reg_write(R_BYPASS, R_BYPASS_DSP_BYPAS);

    regs_write(regs);
    regs_write(size_regs);

    reg_write(BANK_SEL, BANK_SEL_SENSOR);
    reg_write(CLKRC, clkrc);

    reg_write(BANK_SEL, BANK_SEL_DSP);
    reg_write(R_DVP_SP, pclk_div);
    reg_write(R_BYPASS, R_BYPASS_DSP_EN);

    sleep_ms(100);

    // DSP was reset above: re-apply the pixel format
    ov2640_set_pixformat(sensor, s_pixformat);

    s_framesize = framesize;
    return 0;
}

static int ov2640_set_brightness(sensor_t *sensor, int level) {
    (void)sensor;
    reg_write(BANK_SEL, BANK_SEL_DSP);

    level += 3;
    if (level <= 0 || level > NUM_BRIGHTNESS_LEVELS) {
        return -1;
    }
    for (int i = 0; i < 5; i++) {
        reg_write(brightness_regs[0][i], brightness_regs[level][i]);
    }
    return 0;
}

static int ov2640_set_contrast(sensor_t *sensor, int level) {
    (void)sensor;
    reg_write(BANK_SEL, BANK_SEL_DSP);

    level += 3;
    if (level <= 0 || level > NUM_CONTRAST_LEVELS) {
        return -1;
    }
    for (int i = 0; i < 7; i++) {
        reg_write(contrast_regs[0][i], contrast_regs[level][i]);
    }
    return 0;
}

static int ov2640_set_saturation(sensor_t *sensor, int level) {
    (void)sensor;
    reg_write(BANK_SEL, BANK_SEL_DSP);

    level += 3;
    if (level <= 0 || level > NUM_SATURATION_LEVELS) {
        return -1;
    }
    for (int i = 0; i < 5; i++) {
        reg_write(saturation_regs[0][i], saturation_regs[level][i]);
    }
    return 0;
}

// Sharpness control is not supported by the OV2640 (same as esp32-camera)
static int ov2640_set_sharpness(sensor_t *sensor, int level) {
    (void)sensor;
    (void)level;
    return -1;
}

static int ov2640_set_gainceiling(sensor_t *sensor, gainceiling_t gainceiling) {
    (void)sensor;
    if (gainceiling > GAINCEILING_128X) {
        return -1;
    }
    set_reg_bits(BANK_SEL_SENSOR, COM9, 5, 7, gainceiling);
    return 0;
}

static int ov2640_set_whitebal(sensor_t *sensor, int enable) {
    (void)sensor;
    set_reg_bits(BANK_SEL_DSP, CTRL1, 3, 1, enable ? 1 : 0);
    return 0;
}

static int ov2640_set_gain_ctrl(sensor_t *sensor, int enable) {
    (void)sensor;
    set_reg_bits(BANK_SEL_SENSOR, COM8, 0, COM8_AGC_EN, enable ? COM8_AGC_EN : 0);
    return 0;
}

static int ov2640_set_exposure_ctrl(sensor_t *sensor, int enable) {
    (void)sensor;
    set_reg_bits(BANK_SEL_SENSOR, COM8, 0, COM8_AEC_EN, enable ? COM8_AEC_EN : 0);
    return 0;
}

static int ov2640_set_special_effect(sensor_t *sensor, int effect) {
    (void)sensor;
    reg_write(BANK_SEL, BANK_SEL_DSP);

    effect++;
    if (effect <= 0 || effect > NUM_SPECIAL_EFFECTS) {
        return -1;
    }
    for (int i = 0; i < 5; i++) {
        reg_write(special_effects_regs[0][i], special_effects_regs[effect][i]);
    }
    return 0;
}

static int ov2640_set_wb_mode(sensor_t *sensor, int mode) {
    (void)sensor;
    if (mode < 0 || mode > NUM_WB_MODES) {
        return -1;
    }
    // 0xC7 bit 6: 0 = auto WB, 1 = manual WB
    set_reg_bits(BANK_SEL_DSP, 0xC7, 6, 1, mode ? 1 : 0);
    if (mode) {
        reg_write(BANK_SEL, BANK_SEL_DSP);
        for (int i = 0; i < 3; i++) {
            reg_write(wb_modes_regs[0][i], wb_modes_regs[mode][i]);
        }
    }
    return 0;
}

static int ov2640_set_ae_level(sensor_t *sensor, int level) {
    (void)sensor;
    level += 3;
    if (level <= 0 || level > NUM_AE_LEVELS) {
        return -1;
    }
    reg_write(BANK_SEL, BANK_SEL_SENSOR);
    for (int i = 0; i < 3; i++) {
        reg_write(ae_levels_regs[0][i], ae_levels_regs[level][i]);
    }
    return 0;
}

static int ov2640_set_aec_value(sensor_t *sensor, int value) {
    (void)sensor;
    if (value < 0) {
        value = 0;
    } else if (value > 1200) {
        value = 1200;
    }
    // AEC[1:0] in REG04[1:0], AEC[9:2] in AEC, AEC[15:10] in REG45[5:0]
    set_reg_bits(BANK_SEL_SENSOR, REG04, 0, 3, value & 0x3);
    reg_write(BANK_SEL, BANK_SEL_SENSOR);
    reg_write(AEC, (uint8_t)((value >> 2) & 0xFF));
    set_reg_bits(BANK_SEL_SENSOR, REG45, 0, 0x3F, value >> 10);
    return 0;
}

static int ov2640_set_vflip(sensor_t *sensor, int enable) {
    (void)sensor;
    reg_write(BANK_SEL, BANK_SEL_SENSOR);
    uint8_t reg04 = reg_read(REG04);
    reg04 = (reg04 & ~REG04_VFLIP_IMG) | (enable ? REG04_VFLIP_IMG : 0);
    reg_write(REG04, reg04);
    return 0;
}

static int ov2640_set_hmirror(sensor_t *sensor, int enable) {
    (void)sensor;
    reg_write(BANK_SEL, BANK_SEL_SENSOR);
    uint8_t reg04 = reg_read(REG04);
    reg04 = (reg04 & ~REG04_HFLIP_IMG) | (enable ? REG04_HFLIP_IMG : 0);
    reg_write(REG04, reg04);
    return 0;
}

static int ov2640_set_colorbar(sensor_t *sensor, int enable) {
    (void)sensor;
    reg_write(BANK_SEL, BANK_SEL_SENSOR);
    uint8_t com7 = reg_read(COM7);
    com7 = (com7 & ~COM7_COLOR_BAR) | (enable ? COM7_COLOR_BAR : 0);
    reg_write(COM7, com7);
    return 0;
}

static int ov2640_set_quality(sensor_t *sensor, int quality) {
    (void)sensor;
    if (quality < 0) {
        quality = 0;
    }
    if (quality > 63) {
        quality = 63;
    }
    reg_write(BANK_SEL, BANK_SEL_DSP);
    reg_write(QS, (uint8_t)quality);
    return 0;
}

// set_reg/get_reg: reg values >= 0x100 select the sensor bank,
// e.g. set_reg(s, 0x100 | COM7, 0xFF, v). Default is the DSP bank.
static int ov2640_set_reg(sensor_t *sensor, int reg, int mask, int value) {
    (void)sensor;
    reg_write(BANK_SEL, (reg & 0x100) ? BANK_SEL_SENSOR : BANK_SEL_DSP);
    uint8_t r = (uint8_t)(reg & 0xFF);
    uint8_t v = (uint8_t)value;
    if ((mask & 0xFF) != 0xFF) {
        uint8_t old = reg_read(r);
        v = (old & (uint8_t)~mask) | (v & (uint8_t)mask);
    }
    reg_write(r, v);
    return v;
}

static int ov2640_get_reg(sensor_t *sensor, int reg, int mask) {
    (void)sensor;
    reg_write(BANK_SEL, (reg & 0x100) ? BANK_SEL_SENSOR : BANK_SEL_DSP);
    return reg_read((uint8_t)(reg & 0xFF)) & (uint8_t)mask;
}

// ---------------------------------------------------------------------------
// Detection / registration
// ---------------------------------------------------------------------------

int ov2640_detect(sensor_t *sensor) {
    sensor->sccb_addr = OV2640_SCCB_ADDR;

    sreset();
    sleep_ms(50);

    uint16_t pid = read_pid();
    if (pid != OV2640_PID) {
        // one retry, bus may still be settling
        sleep_ms(10);
        pid = read_pid();
    }
    if (pid != OV2640_PID) {
        return -1;
    }
    sensor->id.PID = pid;
    return 0;
}

int ov2640_init_sensor(sensor_t *sensor) {
    sensor->reset            = ov2640_reset;
    sensor->init_status      = NULL;
    sensor->set_pixformat    = ov2640_set_pixformat;
    sensor->set_framesize    = ov2640_set_framesize;
    sensor->set_brightness   = ov2640_set_brightness;
    sensor->set_contrast     = ov2640_set_contrast;
    sensor->set_saturation   = ov2640_set_saturation;
    sensor->set_sharpness    = ov2640_set_sharpness;
    sensor->set_gainceiling  = ov2640_set_gainceiling;
    sensor->set_quality      = ov2640_set_quality;
    sensor->set_colorbar     = ov2640_set_colorbar;
    sensor->set_whitebal     = ov2640_set_whitebal;
    sensor->set_gain_ctrl    = ov2640_set_gain_ctrl;
    sensor->set_exposure_ctrl = ov2640_set_exposure_ctrl;
    sensor->set_hmirror      = ov2640_set_hmirror;
    sensor->set_vflip        = ov2640_set_vflip;
    sensor->set_special_effect = ov2640_set_special_effect;
    sensor->set_wb_mode      = ov2640_set_wb_mode;
    sensor->set_ae_level     = ov2640_set_ae_level;
    sensor->set_aec_value    = ov2640_set_aec_value;
    sensor->set_reg          = ov2640_set_reg;
    sensor->get_reg          = ov2640_get_reg;
    sensor->set_xclk         = NULL;

    // Default register set
    regs_write(ov2640_settings_cif);
    sleep_ms(50);
    return 0;
}
