/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "ov3660.h"

#include "pico/stdlib.h"

#include "../driver/sccb.h"
#include "ov3660_regs.h"
#include "ov3660_settings.h"

// ---------------------------------------------------------------------------
// Internal state (single sensor instance is assumed)
// ---------------------------------------------------------------------------

static pixformat_t s_pixformat = PIXFORMAT_RGB565;
static framesize_t s_framesize = FRAMESIZE_UXGA;
static bool s_binning = false;
static bool s_scale = false;
static bool s_vflip = false;
static bool s_hmirror = false;

// The core starts XCLK at this frequency (DEFAULT_XCLK_HZ in pico_camera.cpp);
// set_xclk is not ported, so the sensor never learns of a different value.
static const uint32_t s_xclk_freq_hz = 10000000;

// ---------------------------------------------------------------------------
// Low-level register access
// ---------------------------------------------------------------------------

static int reg_write(uint16_t reg, uint8_t value) {
    return sccb_write16(OV3660_SCCB_ADDR, reg, value);
}

static uint8_t reg_read(uint16_t reg) {
    uint8_t value = 0;
    sccb_read16(OV3660_SCCB_ADDR, reg, &value);
    return value;
}

static void regs_write(const uint16_t (*regs)[2]) {
    sccb_write_list16(OV3660_SCCB_ADDR, regs);
}

// Read-modify-write of a bit field within a register
static int set_reg_bits(uint16_t reg, uint8_t offset, uint8_t mask, uint8_t value) {
    uint8_t v = reg_read(reg);
    v = (uint8_t)((v & ~(mask << offset)) | ((value & mask) << offset));
    return reg_write(reg, v);
}

// Set or clear all bits of mask depending on enable
static int write_reg_bits(uint16_t reg, uint8_t mask, bool enable) {
    return set_reg_bits(reg, 0, mask, enable ? mask : 0);
}

static uint16_t read_reg16(uint16_t reg) {
    return ((uint16_t)reg_read(reg) << 8) | reg_read(reg + 1);
}

static int write_reg16(uint16_t reg, uint16_t value) {
    if (reg_write(reg, (uint8_t)(value >> 8)) || reg_write(reg + 1, (uint8_t)value)) {
        return -1;
    }
    return 0;
}

static int write_addr_reg(uint16_t reg, uint16_t x_value, uint16_t y_value) {
    if (write_reg16(reg, x_value) || write_reg16(reg + 2, y_value)) {
        return -1;
    }
    return 0;
}

static uint16_t read_pid(void) {
    uint8_t pidh = reg_read(REG_PIDH);
    sleep_ms(1);
    uint8_t pidl = reg_read(REG_PIDL);
    return ((uint16_t)pidh << 8) | pidl;
}

// ---------------------------------------------------------------------------
// sensor_t interface
// ---------------------------------------------------------------------------

static int ov3660_reset(sensor_t *sensor) {
    (void)sensor;
    // Software reset: clear all registers and reset them to default values
    reg_write(SYSTEM_CTROL0, 0x82);
    sleep_ms(100);
    regs_write(ov3660_settings_default);
    // esp32 also calls set_ae_level(0) here; the defaults table above already
    // contains the level-0 AE values, and set_ae_level is not ported yet.
    sleep_ms(100);
    return 0;
}

static int ov3660_set_pixformat(sensor_t *sensor, pixformat_t pixformat) {
    (void)sensor;
    switch (pixformat) {
    case PIXFORMAT_RGB565:
        regs_write(ov3660_settings_rgb565);
        break;
    case PIXFORMAT_YUV422:
        regs_write(ov3660_settings_yuv422);
        break;
    case PIXFORMAT_JPEG:
        regs_write(ov3660_settings_jpeg);
        break;
    default:
        return -1;
    }
    s_pixformat = pixformat;
    sleep_ms(15);
    return 0;
}

// Shared by set_framesize/set_vflip/set_hmirror: compression, binning,
// v-flip and h-mirror bits plus the matching subsample increments.
static int set_image_options(void) {
    int ret = 0;
    uint8_t reg20 = 0;
    uint8_t reg21 = 0;
    uint8_t reg4514 = 0;
    uint8_t reg4514_test = 0;

    // compression
    if (s_pixformat == PIXFORMAT_JPEG) {
        reg21 |= 0x20;
    }

    // binning
    if (s_binning) {
        reg20 |= 0x01;
        reg21 |= 0x01;
        reg4514_test |= 4;
    } else {
        reg20 |= 0x40;
    }

    // V-Flip
    if (s_vflip) {
        reg20 |= 0x06;
        reg4514_test |= 1;
    }

    // H-Mirror
    if (s_hmirror) {
        reg21 |= 0x06;
        reg4514_test |= 2;
    }

    switch (reg4514_test) {
    //no binning
    case 0: reg4514 = 0x88; break;//normal
    case 1: reg4514 = 0x88; break;//v-flip
    case 2: reg4514 = 0xbb; break;//h-mirror
    case 3: reg4514 = 0xbb; break;//v-flip+h-mirror
    //binning
    case 4: reg4514 = 0xaa; break;//normal
    case 5: reg4514 = 0xbb; break;//v-flip
    case 6: reg4514 = 0xbb; break;//h-mirror
    case 7: reg4514 = 0xaa; break;//v-flip+h-mirror
    }

    if (reg_write(TIMING_TC_REG20, reg20)
            || reg_write(TIMING_TC_REG21, reg21)
            || reg_write(0x4514, reg4514)) {
        ret = -1;
    }

    if (s_binning) {
        ret = ret || reg_write(0x4520, 0x0b)
              || reg_write(X_INCREMENT, 0x31)//odd:3, even: 1
              || reg_write(Y_INCREMENT, 0x31);//odd:3, even: 1
    } else {
        ret = ret || reg_write(0x4520, 0xb0)
              || reg_write(X_INCREMENT, 0x11)//odd:1, even: 1
              || reg_write(Y_INCREMENT, 0x11);//odd:1, even: 1
    }
    return ret;
}

static int set_pll(bool bypass, uint8_t multiplier, uint8_t sys_div, uint8_t pre_div,
                   bool root_2x, uint8_t seld5, bool pclk_manual, uint8_t pclk_div) {
    if (multiplier > 31 || sys_div > 15 || pre_div > 3 || pclk_div > 31 || seld5 > 3) {
        return -1;
    }
    int ret = reg_write(SC_PLLS_CTRL0, bypass ? 0x80 : 0x00);
    if (ret == 0) {
        ret = reg_write(SC_PLLS_CTRL1, multiplier & 0x1f);
    }
    if (ret == 0) {
        ret = reg_write(SC_PLLS_CTRL2, 0x10 | (sys_div & 0x0f));
    }
    if (ret == 0) {
        ret = reg_write(SC_PLLS_CTRL3, (pre_div & 0x3) << 4 | seld5 | (root_2x ? 0x40 : 0x00));
    }
    if (ret == 0) {
        ret = reg_write(PCLK_RATIO, pclk_div & 0x1f);
    }
    if (ret == 0) {
        ret = reg_write(VFIFO_CTRL0C, pclk_manual ? 0x22 : 0x20);
    }
    return ret;
}

static int ov3660_set_framesize(sensor_t *sensor, framesize_t framesize) {
    (void)sensor;
    // Our framesize_t tops out at FRAMESIZE_UXGA; esp32 clamps larger
    // sizes to QXGA instead, which does not exist here - just fail.
    if (framesize > FRAMESIZE_UXGA) {
        return -1;
    }

    int ret = 0;
    framesize_t old_framesize = s_framesize;
    s_framesize = framesize;
    uint16_t w = resolution[framesize].width;
    uint16_t h = resolution[framesize].height;
    aspect_ratio_t ratio = resolution[framesize].aspect_ratio;
    const ratio_settings_t *settings = &ratio_table[ratio];

    s_binning = (w <= (settings->max_width / 2) && h <= (settings->max_height / 2));
    s_scale = !((w == settings->max_width && h == settings->max_height)
                || (w == (settings->max_width / 2) && h == (settings->max_height / 2)));

    ret = write_addr_reg(X_ADDR_ST_H, settings->start_x, settings->start_y)
          || write_addr_reg(X_ADDR_END_H, settings->end_x, settings->end_y)
          || write_addr_reg(X_OUTPUT_SIZE_H, w, h);
    if (ret) {
        goto fail;
    }

    if (s_binning) {
        ret = write_addr_reg(X_TOTAL_SIZE_H, settings->total_x, (settings->total_y / 2) + 1)
              || write_addr_reg(X_OFFSET_H, 8, 2);
    } else {
        ret = write_addr_reg(X_TOTAL_SIZE_H, settings->total_x, settings->total_y)
              || write_addr_reg(X_OFFSET_H, 16, 6);
    }

    if (ret == 0) {
        ret = write_reg_bits(ISP_CONTROL_01, 0x20, s_scale);
    }

    if (ret == 0) {
        ret = set_image_options();
    }

    if (ret) {
        goto fail;
    }

    if (s_pixformat == PIXFORMAT_JPEG) {
        // esp32 also checks framesize == FRAMESIZE_QXGA here; not in our enum
        if (s_xclk_freq_hz == 16000000) {
            //40MHz SYSCLK and 10MHz PCLK
            ret = set_pll(false, 24, 1, 3, false, 0, true, 8);
        } else {
            //50MHz SYSCLK and 10MHz PCLK
            ret = set_pll(false, 30, 1, 3, false, 0, true, 10);
        }
    } else {
        //tuned for 16MHz XCLK and 8MHz PCLK
        if (framesize > FRAMESIZE_HVGA) {
            //8MHz SYSCLK and 8MHz PCLK (4.44 FPS)
            ret = set_pll(false, 4, 1, 0, false, 2, true, 2);
        } else if (framesize >= FRAMESIZE_QVGA) {
            //16MHz SYSCLK and 8MHz PCLK (10.25 FPS)
            ret = set_pll(false, 8, 1, 0, false, 2, true, 4);
        } else {
            //32MHz SYSCLK and 8MHz PCLK (17.77 FPS)
            ret = set_pll(false, 8, 1, 0, false, 0, true, 8);
        }
    }

    if (ret == 0) {
        return 0;
    }

fail:
    s_framesize = old_framesize;
    return ret;
}

static int ov3660_set_hmirror(sensor_t *sensor, int enable) {
    (void)sensor;
    s_hmirror = enable ? true : false;
    return set_image_options();
}

static int ov3660_set_vflip(sensor_t *sensor, int enable) {
    (void)sensor;
    s_vflip = enable ? true : false;
    return set_image_options();
}

// Masked multi-byte register access: mask <= 0xFF touches one byte,
// mask > 0xFF a 16-bit big-endian pair at reg, mask > 0xFFFF adds a
// third byte at reg+2 (same convention as esp32-camera).
static int ov3660_set_reg(sensor_t *sensor, int reg, int mask, int value) {
    (void)sensor;
    int old;
    if (mask > 0xFF) {
        old = read_reg16((uint16_t)reg);
        if (mask > 0xFFFF) {
            old = (old << 8) | reg_read((uint16_t)(reg + 2));
        }
    } else {
        old = reg_read((uint16_t)reg);
    }
    value = (old & ~mask) | (value & mask);
    if (mask > 0xFFFF) {
        write_reg16((uint16_t)reg, (uint16_t)(value >> 8));
        reg_write((uint16_t)(reg + 2), (uint8_t)(value & 0xFF));
    } else if (mask > 0xFF) {
        write_reg16((uint16_t)reg, (uint16_t)value);
    } else {
        reg_write((uint16_t)reg, (uint8_t)value);
    }
    return value;
}

static int ov3660_get_reg(sensor_t *sensor, int reg, int mask) {
    (void)sensor;
    int ret;
    if (mask > 0xFF) {
        ret = read_reg16((uint16_t)reg);
        if (mask > 0xFFFF) {
            ret = (ret << 8) | reg_read((uint16_t)(reg + 2));
        }
    } else {
        ret = reg_read((uint16_t)reg);
    }
    return ret & mask;
}

// ---------------------------------------------------------------------------
// Detection / registration
// ---------------------------------------------------------------------------

int ov3660_detect(sensor_t *sensor) {
    sensor->sccb_addr = OV3660_SCCB_ADDR;

    // Software reset first (0x3008 bit 7, as in esp32's reset) so the chip
    // answers from a known state
    reg_write(SYSTEM_CTROL0, 0x82);
    sleep_ms(100);

    uint16_t pid = read_pid();
    if (pid != OV3660_PID) {
        // one retry, bus may still be settling
        sleep_ms(10);
        pid = read_pid();
    }
    if (pid != OV3660_PID) {
        return -1;
    }
    sensor->id.PID = pid;
    return 0;
}

int ov3660_init_sensor(sensor_t *sensor) {
    sensor->reset            = ov3660_reset;
    sensor->init_status      = NULL; // not ported yet
    sensor->set_pixformat    = ov3660_set_pixformat;
    sensor->set_framesize    = ov3660_set_framesize;
    sensor->set_brightness   = NULL; // not ported yet
    sensor->set_contrast     = NULL; // not ported yet
    sensor->set_saturation   = NULL; // not ported yet
    sensor->set_sharpness    = NULL; // not ported yet
    sensor->set_gainceiling  = NULL; // not ported yet
    sensor->set_quality      = NULL; // not ported yet
    sensor->set_colorbar     = NULL; // not ported yet
    sensor->set_whitebal     = NULL; // not ported yet
    sensor->set_gain_ctrl    = NULL; // not ported yet
    sensor->set_exposure_ctrl = NULL; // not ported yet
    sensor->set_hmirror      = ov3660_set_hmirror;
    sensor->set_vflip        = ov3660_set_vflip;
    sensor->set_special_effect = NULL; // not ported yet
    sensor->set_wb_mode      = NULL; // not ported yet
    sensor->set_ae_level     = NULL; // not ported yet
    sensor->set_aec_value    = NULL; // not ported yet
    sensor->set_reg          = ov3660_set_reg;
    sensor->get_reg          = ov3660_get_reg;
    sensor->set_xclk         = NULL;

    // Default register set
    regs_write(ov3660_settings_default);
    sleep_ms(50);
    return 0;
}
