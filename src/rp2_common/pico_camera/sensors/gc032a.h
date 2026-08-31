/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file gc032a.h
 * @brief GC032A sensor driver (8-bit SCCB register addresses).
 *
 * The GC032A has no JPEG encoder: RGB565 only, up to VGA (640x480).
 */
#pragma once

#include "../sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GC032A_SCCB_ADDR    0x21    // 0x42 >> 1
#define GC032A_PID          0x232A

/**
 * @brief Probe the bus for a GC032A. On success fills sensor->id.
 * @return 0 if detected
 */
int gc032a_detect(sensor_t *sensor);

/**
 * @brief Fill sensor_t function pointers and write the default register set.
 * @return 0 on success
 */
int gc032a_init_sensor(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
