/*
 * Copyright (c) 2026 umeko
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "sccb.h"

#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

static i2c_inst_t *s_i2c = NULL;
// When the bus is shared (pin_sda == -1, esp32-camera parity), the caller owns
// it and sccb_deinit() must not tear it down.
static bool s_owns_bus = false;

// Never block forever on a wedged bus (e.g. missing pull-ups or a shorted
// line): all transfers use the _until variants and fail with an error.
#define SCCB_TIMEOUT_US  10000

static absolute_time_t sccb_deadline(void) {
    return make_timeout_time_us(SCCB_TIMEOUT_US);
}

int sccb_init(int i2c_port, int pin_sda, int pin_scl, uint32_t freq_hz) {
    if (i2c_port < 0 || i2c_port > 1 || pin_sda < -1 || (pin_sda >= 0 && pin_scl < 0)) {
        return -1;
    }
    if (pin_sda >= 0) {
        // The I2C pins are hard-muxed: even GPIO = SDA, odd GPIO = SCL, and
        // the pair routes to I2C((pin / 2) % 2). The pins must match the
        // requested peripheral.
        if (pin_sda >= (int)NUM_BANK0_GPIOS || pin_scl >= (int)NUM_BANK0_GPIOS ||
                (pin_sda & 1) || !(pin_scl & 1) ||
                ((pin_sda >> 1) & 1) != i2c_port || ((pin_scl >> 1) & 1) != i2c_port) {
            return -1;
        }
    }
    s_i2c = i2c_get_instance(i2c_port);

    if (pin_sda < 0) {
        // Shared bus mode (esp32-camera parity, pin_sccb_sda == -1): reuse the
        // already initialized I2C peripheral as-is. pin_scl is ignored and the
        // caller is responsible for having set up the bus beforehand
        // (Wire.begin() / Wire1.begin() under Arduino, i2c_init() under the
        // bare Pico SDK).
        s_owns_bus = false;
        return 0;
    }
    s_owns_bus = true;

    gpio_set_function(pin_sda, GPIO_FUNC_I2C);
    gpio_set_function(pin_scl, GPIO_FUNC_I2C);
    gpio_pull_up(pin_sda);
    gpio_pull_up(pin_scl);

    i2c_init(s_i2c, freq_hz);
    return 0;
}

void sccb_deinit(void) {
    if (s_i2c) {
        if (s_owns_bus) {
            i2c_deinit(s_i2c);
        }
        s_i2c = NULL;
        s_owns_bus = false;
    }
}

int sccb_write8(uint8_t slave_addr, uint8_t reg, uint8_t value) {
    if (!s_i2c) {
        return -1;
    }
    uint8_t data[] = { reg, value };
    int ret = i2c_write_blocking_until(s_i2c, slave_addr, data, sizeof(data), false, sccb_deadline());
    return (ret == (int)sizeof(data)) ? 0 : -1;
}

int sccb_read8(uint8_t slave_addr, uint8_t reg, uint8_t *value) {
    if (!s_i2c || !value) {
        return -1;
    }
    // SCCB read: write the register address terminated by a STOP, then read.
    // Do NOT use a repeated start here: strict-SCCB sensors (OV7670) ignore
    // the read phase after a repeated start and never answer. A STOP is the
    // OmniVision SCCB spec behavior and is accepted by OV2640 too.
    int ret = i2c_write_blocking_until(s_i2c, slave_addr, &reg, 1, false, sccb_deadline());
    if (ret < 0) {
        return -1;
    }
    ret = i2c_read_blocking_until(s_i2c, slave_addr, value, 1, false, sccb_deadline());
    return (ret == 1) ? 0 : -1;
}

int sccb_write16(uint8_t slave_addr, uint16_t reg, uint8_t value) {
    if (!s_i2c) {
        return -1;
    }
    uint8_t data[] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF), value };
    int ret = i2c_write_blocking_until(s_i2c, slave_addr, data, sizeof(data), false, sccb_deadline());
    return (ret == (int)sizeof(data)) ? 0 : -1;
}

int sccb_read16(uint8_t slave_addr, uint16_t reg, uint8_t *value) {
    if (!s_i2c || !value) {
        return -1;
    }
    uint8_t addr[] = { (uint8_t)(reg >> 8), (uint8_t)(reg & 0xFF) };
    int ret = i2c_write_blocking_until(s_i2c, slave_addr, addr, sizeof(addr), true, sccb_deadline());
    if (ret < 0) {
        return -1;
    }
    ret = i2c_read_blocking_until(s_i2c, slave_addr, value, 1, false, sccb_deadline());
    return (ret == 1) ? 0 : -1;
}

void sccb_write_list8(uint8_t slave_addr, const uint8_t (*regs)[2]) {
    while (regs) {
        uint8_t reg = (*regs)[0];
        uint8_t value = (*regs)[1];
        if (reg == 0xFF && value == 0xFF) {
            break;
        }
        sccb_write8(slave_addr, reg, value);
        regs++;
    }
}

void sccb_write_list16(uint8_t slave_addr, const uint16_t (*regs)[2]) {
    while (regs) {
        uint16_t reg = (*regs)[0];
        uint8_t value = (uint8_t)(*regs)[1];
        if (reg == 0xFFFF) {
            break;
        }
        if (reg == 0xFFFE) {
            sleep_ms(value);
        } else {
            sccb_write16(slave_addr, reg, value);
        }
        regs++;
    }
}
