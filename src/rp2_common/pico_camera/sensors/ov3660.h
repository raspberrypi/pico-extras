/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file ov3660.h
 * @brief OV3660 sensor driver (16-bit SCCB register addresses).
 */
#pragma once

#include "../sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV3660_SCCB_ADDR    0x3C    // 0x78 >> 1
#define OV3660_PID          0x3660

/**
 * @brief Probe the bus for an OV3660. On success fills sensor->id.
 * @return 0 if detected
 */
int ov3660_detect(sensor_t *sensor);

/**
 * @brief Fill sensor_t function pointers and write the default register set.
 * @return 0 on success
 */
int ov3660_init_sensor(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
