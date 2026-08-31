/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file gc0308.h
 * @brief GC0308 sensor driver (8-bit SCCB register addresses).
 *
 * The GC0308 has no JPEG encoder: RGB565 only, up to VGA (640x480).
 */
#pragma once

#include "../sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GC0308_SCCB_ADDR    0x21    // 0x42 >> 1
#define GC0308_PID          0x9B

/**
 * @brief Probe the bus for a GC0308. On success fills sensor->id.
 * @return 0 if detected
 */
int gc0308_detect(sensor_t *sensor);

/**
 * @brief Fill sensor_t function pointers and write the default register set.
 * @return 0 on success
 */
int gc0308_init_sensor(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
