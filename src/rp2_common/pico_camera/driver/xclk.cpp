/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "xclk.h"

#include "pico/stdlib.h"
#include "hardware/pwm.h"
#include "hardware/gpio.h"
#include "hardware/clocks.h"

int xclk_start(int pin, uint32_t freq_hz) {
    if (pin < 0 || freq_hz == 0) {
        return -1;
    }

    uint32_t sys = clock_get_hz(clk_sys);

    // Choose an integer divider closest to the requested frequency.
    // freq = sys / div, duty 50%.
    uint32_t div = (sys + freq_hz / 2) / freq_hz;
    if (div < 2) {
        div = 2;
    }
    if (div > 65536) {
        div = 65536;
    }

    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice = pwm_gpio_to_slice_num(pin);

    // wrap = div - 1, level = div / 2  →  ~50% duty
    pwm_set_wrap(slice, div - 1);
    pwm_set_gpio_level(pin, div / 2);
    pwm_set_enabled(slice, true);

    // Let the clock stabilize before the sensor uses it
    sleep_ms(100);
    return 0;
}

void xclk_stop(int pin) {
    if (pin < 0) {
        return;
    }
    uint slice = pwm_gpio_to_slice_num(pin);
    pwm_set_enabled(slice, false);
    gpio_set_function(pin, GPIO_FUNC_SIO);
}
