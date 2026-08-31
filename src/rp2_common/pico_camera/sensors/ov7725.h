/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file ov7725.h
 * @brief OV7725 sensor driver (8-bit SCCB register addresses).
 *
 * The OV7725 has no JPEG encoder: RGB565 only, up to VGA.
 */
#pragma once

#include "../sensor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define OV7725_SCCB_ADDR    0x21    // 0x42 >> 1
#define OV7725_PID          0x77

/**
 * @brief Probe the bus for an OV7725. On success fills sensor->id.
 * @return 0 if detected
 */
int ov7725_detect(sensor_t *sensor);

/**
 * @brief Fill sensor_t function pointers and write the default register set.
 * @return 0 on success
 */
int ov7725_init_sensor(sensor_t *sensor);

#ifdef __cplusplus
}
#endif
