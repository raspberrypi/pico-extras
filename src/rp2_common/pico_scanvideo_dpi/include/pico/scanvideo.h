/*
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef PICO_SCANVIDEO_H_
#define PICO_SCANVIDEO_H_

// note that defining to false will force non-inclusion also
#if !defined(PICO_SCANVIDEO_DPI)
#define PICO_SCANVIDEO_DPI 1

#ifndef PARAM_ASSERTIONS_ENABLED_SCANVIDEO_DPI
#define PARAM_ASSERTIONS_ENABLED_SCANVIDEO_DPI 0
#endif

#include "pico/scanvideo/scanvideo_base.h"

#ifndef PICO_SCANVIDEO_DPI_ALPHA_PIN
#define PICO_SCANVIDEO_DPI_ALPHA_PIN 5u
#endif

#ifndef PICO_SCANVIDEO_DPI_PIXEL_RSHIFT
#define PICO_SCANVIDEO_DPI_PIXEL_RSHIFT 0u
#endif

#ifndef PICO_SCANVIDEO_DPI_PIXEL_GSHIFT
#define PICO_SCANVIDEO_DPI_PIXEL_GSHIFT 6u
#endif

#ifndef PICO_SCANVIDEO_DPI_PIXEL_BSHIFT
#define PICO_SCANVIDEO_DPI_PIXEL_BSHIFT 11u
#endif

#ifndef PICO_SCANVIDEO_ALPHA_PIN
#define PICO_SCANVIDEO_ALPHA_PIN PICO_SCANVIDEO_DPI_ALPHA_PIN
#endif

#ifndef PICO_SCANVIDEO_PIXEL_RSHIFT
#define PICO_SCANVIDEO_PIXEL_RSHIFT PICO_SCANVIDEO_DPI_PIXEL_RSHIFT
#endif

#ifndef PICO_SCANVIDEO_PIXEL_GSHIFT
#define PICO_SCANVIDEO_PIXEL_GSHIFT PICO_SCANVIDEO_DPI_PIXEL_GSHIFT
#endif

#ifndef PICO_SCANVIDEO_PIXEL_BSHIFT
#define PICO_SCANVIDEO_PIXEL_BSHIFT PICO_SCANVIDEO_DPI_PIXEL_BSHIFT
#endif

/** \file scanvideo.h
 *  \defgroup pico_scanvideo_dpi pico_scanvideo_dpi
 *
 * DPI Scan-out Video using the PIO
 */

#endif
#endif
