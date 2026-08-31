/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * GC0308 register definitions (8-bit register addresses).
 * Ported from esp32-camera's private_include/gc0308_regs.h (Apache-2.0),
 * comments preserved as-is.
 */
#ifndef __GC0308_REGS_H__
#define __GC0308_REGS_H__

// System Control Registers
#define RESET_RELATED   0xfe    // Bit[7]: Software reset
                                // Bit[6:5]: NA
                                // Bit[4]: CISCTL_restart_n
                                // Bit[3:1]: NA
                                // Bit[0]: page select
                                //  0:page0
                                //  1:page1

// Page 0 Registers

// Exposure Control (Manual, when AEC disabled)
#define EXPOSURE_HIGH   0x03    // Exposure time high byte (bits 11:4)
#define EXPOSURE_LOW    0x04    // Exposure time low byte (bits 3:0 in upper nibble)
                                // Total exposure = (EXPOSURE_HIGH << 4) | (EXPOSURE_LOW >> 4)
                                // Range: 0-4095 (12-bit value)

// Windowing Registers
#define ROW_START_H     0x05    // Row start address high byte
#define ROW_START_L     0x06    // Row start address low byte
#define COL_START_H     0x07    // Column start address high byte
#define COL_START_L     0x08    // Column start address low byte
#define WIN_HEIGHT_H    0x09    // Window height high byte
#define WIN_HEIGHT_L    0x0a    // Window height low byte
#define WIN_WIDTH_H     0x0b    // Window width high byte
#define WIN_WIDTH_L     0x0c    // Window width low byte

// Sensor Control
#define CISCTL_MODE1    0x14    // Bit[1]: V_flip, Bit[0]: H_mirror

// AAAA_EN - Auto Algorithm Enable (AEC/AGC/AWB/ABB)
#define AAAA_EN         0x22    // Auto control enable register
                                // Per Linux kernel driver:
                                // Bit[0]: AEC enable (1=on, 0=off)
                                // Bit[1]: AWB enable (1=on, 0=off)
                                // Bit[2]: AGC enable (1=on, 0=off)

// Special Effects and Output Control
#define SPECIAL_EFFECT  0x23    // Special effect control
                                // Bit[1:0]: Effect mode
                                //  00: Normal
                                //  01: Negative
                                //  10: Grayscale/Color tint mode

// Output Format
#define OUTPUT_FMT      0x24    // Output format control
                                // Bit[3:0]: Output format selection
                                //  0010: YCbYCr (YUV422)
                                //  0110: RGB565
                                //  others: see datasheet

// Analog Circuit Control
#define PCLK_DIV        0x28    // PCLK divider control (frequency division)

// Output Control
#define OUT_CTRL        0x2e    // Output control
                                // Bit[0]: Color bar enable

// Global Gain
#define GLOBAL_GAIN     0x50    // Global gain control (hardware: 0-63, 6-bit)

// Manual White Balance Gain (when AWB disabled)
#define AWB_R_GAIN      0x5a    // Red channel gain for manual WB
#define AWB_G_GAIN      0x5b    // Green channel gain for manual WB
#define AWB_B_GAIN      0x5c    // Blue channel gain for manual WB

// Edge Enhancement (INTPEE - Interpolation and Edge-Enhancement)
#define EDGE12_EFFECT   0x77    // Edge enhancement effect control
#define EDGE_POS_RATIO  0x78    // Edge position ratio
#define EDGE1_MINMAX    0x79    // Edge1 min/max control

// Saturation Control (Page 0)
#define SATURATION_Cb   0xb1    // Cb saturation gain (default: 0x40)
#define SATURATION_Cr   0xb2    // Cr saturation gain (default: 0x40)

// Contrast
#define CONTRAST        0xb3    // Contrast control (default: 0x40)

// AEC Registers - Automatic Exposure Control (Page 0)
#define AEC_MODE1       0xd0    // AEC mode control register 1
#define AEC_MODE2       0xd1    // AEC mode control register 2
#define AEC_MODE3       0xd2    // AEC mode control register 3
#define AEC_TARGET_Y    0xd3    // AEC target Y value / Expected luminance (default: 0x48)

// Subsample Window (Page 0 for coordinate reference)
#define SUB_COL_N       0xf7    // Subsample column start (units of 4 pixels)
#define SUB_ROW_N       0xf8    // Subsample row start (units of 4 pixels)
#define SUB_COL_N1      0xf9    // Subsample column end (units of 4 pixels)
#define SUB_ROW_N1      0xfa    // Subsample row end (units of 4 pixels)

// Page 1 Registers (access after setting 0xfe = 0x01)

// Subsample Control (Page 1)
#define SUBSAMPLE_EN    0x53    // Bit[7]: Subsample enable
#define SUBSAMPLE_MODE  0x54    // Subsample mode control
#define SUBSAMPLE_EN2   0x55    // Bit[0]: Subsample enable 2
#define SUBSAMPLE_Y0    0x56    // Y subsample config 0
#define SUBSAMPLE_Y1    0x57    // Y subsample config 1
#define SUBSAMPLE_UV0   0x58    // UV subsample config 0
#define SUBSAMPLE_UV1   0x59    // UV subsample config 1

/**
 * @brief Register bit masks
 */

// AAAA_EN (0x22) bit masks - Auto Algorithm Enable
#define AEC_ENABLE      0x01    // Bit 0: AEC enable
#define AWB_ENABLE      0x02    // Bit 1: AWB enable
#define AGC_ENABLE      0x04    // Bit 2: AGC enable

// CISCTL_MODE1 (0x14) bit masks
#define HMIRROR_MASK    0x01    // Bit 0: Horizontal mirror
#define VFLIP_MASK      0x02    // Bit 1: Vertical flip

// SPECIAL_EFFECT (0x23) bit masks
#define EFFECT_NORMAL   0x00    // Normal mode
#define EFFECT_NEGATIVE 0x01    // Negative/inverse
#define EFFECT_GRAYSCALE 0x02   // B&W or color tint mode

// OUT_CTRL (0x2e) bit masks
#define COLORBAR_ENABLE 0x01    // Bit 0: Color bar test pattern

#endif // __GC0308_REGS_H__
