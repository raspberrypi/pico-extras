/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file ov2640.h
 * @brief OV2640 sensor driver (8-bit SCCB register addresses).
 */
#pragma once

#include "../sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV2640_SCCB_ADDR    0x30    // 0x60 >> 1
#define OV2640_PID          0x2642

/**
 * @brief Probe the bus for an OV2640. On success fills sensor->id.
 * @return 0 if detected
 */
int ov2640_detect(sensor_t *sensor);

/**
 * @brief Fill sensor_t function pointers and write the default register set.
 * @return 0 on success
 */
int ov2640_init_sensor(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
