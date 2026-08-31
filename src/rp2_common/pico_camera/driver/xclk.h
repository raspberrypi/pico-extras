/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

/**
 * @file xclk.h
 * @brief XCLK generation via PWM.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start the camera master clock on a GPIO.
 *
 * Uses the PWM slice of the given pin. Frequency is derived from the
 * actual sys clock, so overclocked boards get the right frequency.
 * The achievable granularity near 20 MHz is limited (sys_clk/N);
 * OV sensors tolerate a wide range, so the closest divider is used.
 *
 * @param pin      GPIO number
 * @param freq_hz  requested frequency, typically 10-24 MHz
 * @return 0 on success
 */
int  xclk_start(int pin, uint32_t freq_hz);

/** Stop the clock and release the pin. */
void xclk_stop(int pin);

#ifdef __cplusplus
}
#endif
