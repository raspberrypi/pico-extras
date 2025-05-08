/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This header file declares a function used to access the WiFi settings
 * and other key/value data in Flash.
 *
 */

#ifndef _WIFI_SETTINGS_FLASH_STORAGE_H_
#define _WIFI_SETTINGS_FLASH_STORAGE_H_

#include "pico/stdlib.h"
#include <stdbool.h>
#include <stdint.h>

/// @brief Scan the settings file in Flash for a particular key.
/// If found, copy up to *value_size characters to value.
/// Note: value will not be '\0' terminated.
/// @param[in] key Key to be found ('\0' terminated)
/// @param[out] value Value for key (if found) - not '\0' terminated
/// @param[inout] value_size Size of the value
/// @return true if key found
/// @details This function has a weak symbol, allowing it to be reimplemented
/// by applications in order to load settings from some other storage
bool wifi_settings_get_value_for_key(
            const char* key,
            char* value, uint* value_size);

#endif
