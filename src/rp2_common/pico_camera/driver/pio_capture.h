/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file pio_capture.h
 * @brief PIO + DMA frame capture engine.
 *
 * The PIO program is assembled at runtime with pio_encode_*(), so all
 * pins come from camera_config_t - there is no .pio file and no pioasm
 * build step (which also keeps the library Arduino-IDE friendly).
 */
#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int pin_d0;     //< first of 8 consecutive data pins (d0..d0+7)
    int pin_vsync;
    int pin_href;
    int pin_pclk;
} pio_capture_pins_t;

/**
 * @brief Claim a PIO state machine and DMA channel, load the capture program.
 * @return 0 on success
 */
int  pio_capture_init(const pio_capture_pins_t *pins);

/** Release PIO state machine and DMA channel. */
void pio_capture_deinit(void);

/**
 * @brief Capture exactly len bytes (one frame) into buf, blocking.
 *
 * Waits for the next VSYNC rising edge, then captures while HREF is high,
 * clocked by PCLK, until len bytes are in the buffer.
 *
 * @param timeout_ms  abort and return PICO timeout error after this long
 * @return 0 on success, negative on timeout/error
 */
int  pio_capture_frame(uint8_t *buf, size_t len, uint32_t timeout_ms);

/**
 * @brief Capture one variable-length frame (JPEG) into buf.
 *
 * Uses the continuous capture program: streams from a VSYNC rising edge
 * until VSYNC falls again (frame end), then trims the buffer at the JPEG
 * EOI marker (0xFFD9).
 *
 * @param buf        destination buffer
 * @param capacity   buffer capacity in bytes; a frame larger than this
 *                   is truncated
 * @param out_len    receives the actual frame length in bytes
 * @param timeout_ms abort and return error after this long
 * @return 0 on success, negative on timeout/error
 */
int  pio_capture_frame_variable(uint8_t *buf, size_t capacity, size_t *out_len, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
