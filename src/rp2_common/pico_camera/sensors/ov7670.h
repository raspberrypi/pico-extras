/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file ov7670.h
 * @brief OV7670 sensor driver (8-bit SCCB register addresses).
 *
 * The OV7670 has no JPEG encoder: RGB565 only, up to VGA.
 */
#pragma once

#include "../sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV7670_SCCB_ADDR    0x21    // 0x42 >> 1
#define OV7670_PID          0x76

/**
 * @brief Probe the bus for an OV7670. On success fills sensor->id.
 * @return 0 if detected
 */
int ov7670_detect(sensor_t *sensor);

/**
 * @brief Fill sensor_t function pointers and write the default register set.
 * @return 0 on success
 */
int ov7670_init_sensor(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
