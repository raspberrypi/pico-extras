/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file sensor.h
 * @brief Common camera sensor types, aligned with esp32-camera's sensor.h (subset).
 *
 * Naming and semantics follow esp32-camera so that existing code and
 * documentation carry over with minimal changes.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PIXFORMAT_RGB565,   // 2 bytes per pixel
    PIXFORMAT_YUV422,   // 2 bytes per pixel (YUYV)
    PIXFORMAT_GRAYSCALE,// 1 byte per pixel (Y only)
    PIXFORMAT_JPEG,     // compressed stream
    PIXFORMAT_INVALID
} pixformat_t;

typedef enum {
    FRAMESIZE_96X96,    // 96x96
    FRAMESIZE_QQVGA,    // 160x120
    FRAMESIZE_QCIF,     // 176x144
    FRAMESIZE_HQVGA,    // 240x176
    FRAMESIZE_240X240,  // 240x240
    FRAMESIZE_QVGA,     // 320x240
    FRAMESIZE_CIF,      // 400x296
    FRAMESIZE_HVGA,     // 480x320
    FRAMESIZE_VGA,      // 640x480
    FRAMESIZE_SVGA,     // 800x600
    FRAMESIZE_XGA,      // 1024x768
    FRAMESIZE_HD,       // 1280x720
    FRAMESIZE_SXGA,     // 1280x1024
    FRAMESIZE_UXGA,     // 1600x1200
    FRAMESIZE_INVALID
} framesize_t;

typedef enum {
    ASPECT_RATIO_4X3,
    ASPECT_RATIO_3X2,
    ASPECT_RATIO_16X10,
    ASPECT_RATIO_5X3,
    ASPECT_RATIO_16X9,
    ASPECT_RATIO_21X9,
    ASPECT_RATIO_5X4,
    ASPECT_RATIO_1X1,
    ASPECT_RATIO_9X16
} aspect_ratio_t;

typedef enum {
    GAINCEILING_2X,
    GAINCEILING_4X,
    GAINCEILING_8X,
    GAINCEILING_16X,
    GAINCEILING_32X,
    GAINCEILING_64X,
    GAINCEILING_128X,
} gainceiling_t;

typedef struct {
    const uint16_t width;
    const uint16_t height;
    const aspect_ratio_t aspect_ratio;
} resolution_info_t;

/** Resolution table, indexed by framesize_t */
static const resolution_info_t resolution[FRAMESIZE_INVALID] = {
    {   96,   96, ASPECT_RATIO_1X1   }, /* 96x96 */
    {  160,  120, ASPECT_RATIO_4X3   }, /* QQVGA */
    {  176,  144, ASPECT_RATIO_5X4   }, /* QCIF  */
    {  240,  176, ASPECT_RATIO_4X3   }, /* HQVGA */
    {  240,  240, ASPECT_RATIO_1X1   }, /* 240x240 */
    {  320,  240, ASPECT_RATIO_4X3   }, /* QVGA  */
    {  400,  296, ASPECT_RATIO_4X3   }, /* CIF   */
    {  480,  320, ASPECT_RATIO_3X2   }, /* HVGA  */
    {  640,  480, ASPECT_RATIO_4X3   }, /* VGA   */
    {  800,  600, ASPECT_RATIO_4X3   }, /* SVGA  */
    { 1024,  768, ASPECT_RATIO_4X3   }, /* XGA   */
    { 1280,  720, ASPECT_RATIO_16X9  }, /* HD    */
    { 1280, 1024, ASPECT_RATIO_5X4   }, /* SXGA  */
    { 1600, 1200, ASPECT_RATIO_4X3   }, /* UXGA  */
};

typedef struct {
    uint8_t MIDH;
    uint8_t MIDL;
    uint16_t PID;
    uint8_t VER;
} sensor_id_t;

typedef struct {
    const char *name;
    const uint8_t sccb_addr;
    const uint16_t pid;
    const framesize_t max_size;
    const bool support_jpeg;
} camera_sensor_info_t;

typedef struct sensor_s sensor_t;

/**
 * @brief Sensor control structure, aligned with esp32-camera.
 *
 * Function pointers that a sensor model does not implement are NULL.
 * Callers must check for NULL before use.
 */
struct sensor_s {
    sensor_id_t id;             //< filled by detect()
    uint8_t sccb_addr;          //< 7-bit SCCB slave address

    int (*reset)(sensor_t *sensor);
    int (*init_status)(sensor_t *sensor);
    int (*set_pixformat)(sensor_t *sensor, pixformat_t pixformat);
    int (*set_framesize)(sensor_t *sensor, framesize_t framesize);
    int (*set_brightness)(sensor_t *sensor, int level);
    int (*set_contrast)(sensor_t *sensor, int level);
    int (*set_saturation)(sensor_t *sensor, int level);
    int (*set_sharpness)(sensor_t *sensor, int level);
    int (*set_gainceiling)(sensor_t *sensor, gainceiling_t gainceiling);
    int (*set_quality)(sensor_t *sensor, int quality);
    int (*set_colorbar)(sensor_t *sensor, int enable);
    int (*set_whitebal)(sensor_t *sensor, int enable);
    int (*set_gain_ctrl)(sensor_t *sensor, int enable);
    int (*set_exposure_ctrl)(sensor_t *sensor, int enable);
    int (*set_hmirror)(sensor_t *sensor, int enable);
    int (*set_vflip)(sensor_t *sensor, int enable);
    int (*set_special_effect)(sensor_t *sensor, int effect);
    int (*set_wb_mode)(sensor_t *sensor, int mode);
    int (*set_ae_level)(sensor_t *sensor, int level);
    int (*set_aec_value)(sensor_t *sensor, int value);

    /** Write masked bits of a register. mask selects which bits to touch. */
    int (*set_reg)(sensor_t *sensor, int reg, int mask, int value);
    /** Read a register, optionally masked. Returns negative on error. */
    int (*get_reg)(sensor_t *sensor, int reg, int mask);

    /** Change XCLK frequency at runtime. timer is unused on RP2040. */
    int (*set_xclk)(sensor_t *sensor, int timer, int xclk);
};

#ifdef __cplusplus
}
#endif
