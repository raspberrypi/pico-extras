/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file gc2145.h
 * @brief GC2145 sensor driver (8-bit SCCB register addresses).
 *
 * The GC2145 has no JPEG encoder: RGB565 only, up to UXGA (1600x1200).
 */
#pragma once

#include "../sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GC2145_SCCB_ADDR    0x3C    // 0x78 >> 1
#define GC2145_PID          0x2145

/**
 * @brief Probe the bus for a GC2145. On success fills sensor->id.
 * @return 0 if detected
 */
int gc2145_detect(sensor_t *sensor);

/**
 * @brief Fill sensor_t function pointers and write the default register set.
 * @return 0 on success
 */
int gc2145_init_sensor(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
