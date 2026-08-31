/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file pico_camera.h
 * @brief PicoCamera main API, aligned with esp32-camera's esp_camera.h.
 *
 * Example use:
 *
 *     camera_config_t config = {
 *         .pin_pwdn     = -1,
 *         .pin_reset    = 29,
 *         .pin_xclk     = 5,
 *         .pin_sccb_sda = 12,
 *         .pin_sccb_scl = 13,
 *         .pin_d0 = 14, .pin_d1 = 15, .pin_d2 = 16, .pin_d3 = 17,
 *         .pin_d4 = 18, .pin_d5 = 19, .pin_d6 = 20, .pin_d7 = 21,
 *         .pin_vsync = 2, .pin_href = 3, .pin_pclk = 4,
 *         .xclk_freq_hz = 10000000,
 *         .sccb_i2c_port = 0,
 *         .pixel_format = PIXFORMAT_RGB565,
 *         .frame_size   = FRAMESIZE_QVGA,
 *         .jpeg_quality = 10,
 *         .fb_count     = 1,
 *         .fb_location  = PICO_CAMERA_FB_AUTO,
 *     };
 *
 *     pico_camera_init(&config);
 *     camera_fb_t *fb = pico_camera_fb_get();
 *     // use fb->buf / fb->len / fb->width / fb->height
 *     pico_camera_fb_return(fb);
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/time.h>

#include "sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int pico_camera_err_t;

#define PICO_CAMERA_OK                            0
#define PICO_CAMERA_FAIL                          -1
#define PICO_CAMERA_ERR_NO_MEM                    -2
#define PICO_CAMERA_ERR_INVALID_ARG               -3
#define PICO_CAMERA_ERR_INVALID_STATE             -4
#define PICO_CAMERA_ERR_NOT_SUPPORTED             -5
#define PICO_CAMERA_ERR_TIMEOUT                   -6
#define PICO_CAMERA_ERR_BASE                      (-100)
#define PICO_CAMERA_ERR_NOT_DETECTED              (PICO_CAMERA_ERR_BASE - 1)
#define PICO_CAMERA_ERR_FAILED_TO_SET_FRAME_SIZE  (PICO_CAMERA_ERR_BASE - 2)
#define PICO_CAMERA_ERR_FAILED_TO_SET_OUT_FORMAT  (PICO_CAMERA_ERR_BASE - 3)

/**
 * @brief Human-readable description of a pico_camera error code.
 *
 * Handy for logging: Serial.printf("init failed: %s\n", pico_camera_err_str(err));
 */
static inline const char *pico_camera_err_str(pico_camera_err_t err) {
    switch (err) {
    case PICO_CAMERA_OK:                           return "OK";
    case PICO_CAMERA_FAIL:                         return "generic failure";
    case PICO_CAMERA_ERR_NO_MEM:                   return "out of memory (frame buffer allocation failed)";
    case PICO_CAMERA_ERR_INVALID_ARG:              return "invalid argument (check pins, pixel_format, frame_size)";
    case PICO_CAMERA_ERR_INVALID_STATE:            return "invalid state (camera already initialized)";
    case PICO_CAMERA_ERR_NOT_SUPPORTED:            return "not supported (e.g. JPEG on a sensor without a JPEG encoder)";
    case PICO_CAMERA_ERR_TIMEOUT:                  return "timeout";
    case PICO_CAMERA_ERR_NOT_DETECTED:             return "no sensor detected on the SCCB bus (check wiring/power)";
    case PICO_CAMERA_ERR_FAILED_TO_SET_FRAME_SIZE: return "failed to set frame size";
    case PICO_CAMERA_ERR_FAILED_TO_SET_OUT_FORMAT: return "failed to set output format";
    default:                                       return "unknown error";
    }
}

/* Library version, kept in sync with library.properties / library.json.
 * When a copy installed from the Library Manager overrides a (possibly older)
 * copy bundled with the Arduino core, these macros describe the copy the
 * sketch actually compiled against. */
#define PICO_CAMERA_VERSION_MAJOR   0
#define PICO_CAMERA_VERSION_MINOR   4
#define PICO_CAMERA_VERSION_PATCH   2
#define PICO_CAMERA_VERSION_HEX     ((PICO_CAMERA_VERSION_MAJOR << 16) | \
                                     (PICO_CAMERA_VERSION_MINOR << 8) |  \
                                     PICO_CAMERA_VERSION_PATCH)
#define PICO_CAMERA_VERSION_STRING  "0.4.2"

/**
 * @brief Frame buffer location
 *
 * PSRAM requires an RP2350 board with a PSRAM chip, enabled via the core's
 * PSRAM menu (Arduino IDE) or board_upload.psram_length (PlatformIO);
 * RP2040 has no PSRAM support. Unlike esp32-camera, PICO_CAMERA_FB_AUTO
 * falls back to SRAM when PSRAM allocation fails — esp32-camera has no
 * automatic fallback and fails init instead.
 */
typedef enum {
    PICO_CAMERA_FB_AUTO = 0,        //< PSRAM when available, SRAM otherwise (default)
    PICO_CAMERA_FB_IN_PSRAM,        //< PSRAM only; init fails if unavailable (esp32-camera parity)
    PICO_CAMERA_FB_IN_SRAM          //< On-chip SRAM only
} camera_fb_location_t;

/**
 * @brief Configuration structure for camera initialization
 *
 * Pin fields mirror esp32-camera. Note the RP2040 PIO constraint:
 * pin_d0..pin_d7 must be 8 consecutive GPIOs (pin_dN == pin_d0 + N).
 * pin_vsync / pin_href / pin_pclk are free choice.
 * A pin value of -1 means "not connected" (pin_pwdn / pin_reset) or, for
 * pin_sccb_sda, "reuse an already initialized I2C bus" (see that field).
 */
typedef struct {
    int pin_pwdn;                   //< GPIO for camera power down line (-1 if unused)
    int pin_reset;                  //< GPIO for camera reset line (-1 if unused)
    int pin_xclk;                   //< GPIO for XCLK output
    int pin_sccb_sda;               //< GPIO for SCCB SDA. Hard-muxed: SDA must be an even
    //< GPIO, SCL the matching odd one, both routing to I2C(sccb_i2c_port) —
    //< i.e. (pin / 2) % 2 == sccb_i2c_port (e.g. port 0: SDA on 0/4/8/.../28,
    //< SCL on 1/5/9/.../29). If -1, use the already configured I2C bus by
    //< number (sccb_i2c_port); esp32-camera parity. The bus must be
    //< initialized beforehand (Wire.begin() / Wire1.begin() under Arduino,
    //< i2c_init() under the bare Pico SDK).
    int pin_sccb_scl;               //< GPIO for SCCB SCL (ignored when pin_sccb_sda is -1)
    int pin_d0;                     //< GPIO for data line 0 (lowest of 8 consecutive pins)
    int pin_d1;
    int pin_d2;
    int pin_d3;
    int pin_d4;
    int pin_d5;
    int pin_d6;
    int pin_d7;
    int pin_vsync;                  //< GPIO for VSYNC line
    int pin_href;                   //< GPIO for HREF line
    int pin_pclk;                   //< GPIO for PCLK line

    int xclk_freq_hz;               //< XCLK frequency in Hz (0 = default 10 MHz)

    int sccb_i2c_port;              //< I2C peripheral: 0 or 1. In shared bus mode
    //< (pin_sccb_sda == -1) this selects which
    //< already-initialized bus to reuse

    pixformat_t pixel_format;       //< PIXFORMAT_RGB565, PIXFORMAT_YUV422 or PIXFORMAT_JPEG
    framesize_t frame_size;         //< FRAMESIZE_*

    int jpeg_quality;               //< 0-63, lower means higher quality (JPEG only)
    size_t fb_count;                //< Number of frame buffers to allocate
    camera_fb_location_t fb_location; //< Where frame buffers live; PICO_CAMERA_FB_AUTO (0)
    //< keeps old zero-initialized configs working
} camera_config_t;

/**
 * @brief Data structure of camera frame buffer
 */
typedef struct {
    uint8_t *buf;                   //< Pointer to the pixel data
    size_t len;                     //< Length of the used data in bytes
    size_t width;                   //< Width in pixels
    size_t height;                  //< Height in pixels
    pixformat_t format;             //< Format of the pixel data
    struct timeval timestamp;       //< Timestamp since boot of the captured frame
} camera_fb_t;

/**
 * @brief Initialize the camera driver
 *
 * Detects the sensor over SCCB, allocates frame buffers, sets up PIO + DMA.
 * Can only be called once until pico_camera_deinit().
 *
 * @param config Camera configuration parameters
 * @return PICO_CAMERA_OK on success.
 *         PICO_CAMERA_ERR_NOT_SUPPORTED if PIXFORMAT_JPEG is requested on a
 *         sensor without a JPEG encoder (see camera_sensor_info_t.support_jpeg;
 *         esp32-camera behaves the same way). A frame_size beyond the sensor's
 *         maximum is clamped to the maximum instead of failing.
 */
pico_camera_err_t pico_camera_init(const camera_config_t *config);

/**
 * @brief Deinitialize the camera driver and free all resources
 */
pico_camera_err_t pico_camera_deinit(void);

/**
 * @brief Obtain a frame buffer with a freshly captured frame.
 *
 * Blocks until the frame is captured. Returns NULL on error or if no
 * free buffer is available (return buffers with pico_camera_fb_return()).
 */
camera_fb_t *pico_camera_fb_get(void);

/**
 * @brief Return the frame buffer to be reused again.
 */
void pico_camera_fb_return(camera_fb_t *fb);

/**
 * @brief Get a pointer to the image sensor control structure
 *
 * @return pointer to the sensor, NULL if not initialized
 */
sensor_t *pico_camera_sensor_get(void);

/**
 * @brief Get information about the detected sensor model
 *
 * @return pointer to static info, NULL if not initialized
 */
const camera_sensor_info_t *pico_camera_sensor_info_get(void);

#ifdef __cplusplus
}
#endif
