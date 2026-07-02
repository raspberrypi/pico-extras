/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _HARDWARE_ROSC_EXTRA_H_
#define _HARDWARE_ROSC_EXTRA_H_

#include "pico.h"
#include "hardware/rosc.h"
#include "hardware/structs/rosc.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \file rosc_extra.h
 *  \defgroup hardware_rosc hardware_rosc
 *
 * Ring Oscillator (ROSC) API Extras
 *
 * A Ring Oscillator is an on-chip oscillator that requires no external crystal. Instead, the output is generated from a series of
 * inverters that are chained together to create a feedback loop. RP2040 boots from the ring oscillator initially, meaning the
 * first stages of the bootrom, including booting from SPI flash, will be clocked by the ring oscillator. If your design has a
 * crystal oscillator, you’ll likely want to switch to this as your reference clock as soon as possible, because the frequency is
 * more accurate than the ring oscillator.
 */

/*! \brief  Set frequency of the Ring Oscillator
 *  \ingroup hardware_rosc
 *
 * \param code The drive strengths. See the RP2040 datasheet for information on this value.
 */
void rosc_set_freq(uint32_t code);

/*! \brief  Set range of the Ring Oscillator
 *  \ingroup hardware_rosc
 *
 * Frequency range. Frequencies will vary with Process, Voltage & Temperature (PVT).
 * Clock output will not glitch when changing the range up one step at a time.
 *
 * \param range 0x01 Low, 0x02 Medium, 0x03 High, 0x04 Too High.
 */
void rosc_set_range(uint range);

// FIXME: Add doxygen

uint32_t next_rosc_code(uint32_t code);

uint rosc_find_freq(uint32_t low_mhz, uint32_t high_mhz);

void rosc_set_div(uint32_t div);

#ifdef __cplusplus
}
#endif

#endif