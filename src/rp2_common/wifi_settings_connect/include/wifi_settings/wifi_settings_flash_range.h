/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This header file declares functions used to check and translate address ranges.
 *
 */

#ifndef _WIFI_SETTINGS_FLASH_RANGE_H_
#define _WIFI_SETTINGS_FLASH_RANGE_H_

#include <stdbool.h>
#include <stdint.h>

/// @brief Represents a range of Flash memory addresses
typedef struct wifi_settings_flash_range_t {
    uint32_t start_address;
    uint32_t size;
} wifi_settings_flash_range_t;

/// @brief Represents a range of logical memory addresses
typedef struct wifi_settings_logical_range_t {
    void* start_address;
    uint32_t size;
} wifi_settings_logical_range_t;

/// @brief Detect if a Flash memory range entirely fits within another
/// @param[in] inner Inner Flash memory range
/// @param[in] outer Outer Flash memory range
/// @return true if inner is entirely within outer
bool wifi_settings_range_is_contained(
    const wifi_settings_flash_range_t* inner,
    const wifi_settings_flash_range_t* outer);
    
/// @brief Detect if a Flash memory range intersects with another
/// @param[in] fr1 First range
/// @param[in] fr2 Second range
/// @return true if fr1 and fr2 overlap by one or more bytes
bool wifi_settings_range_has_overlap(
    const wifi_settings_flash_range_t* fr1,
    const wifi_settings_flash_range_t* fr2);

/// @brief Determine the range of addresses that are in Flash
/// @param[out] r Flash memory range
void wifi_settings_range_get_all(
    wifi_settings_flash_range_t* r);

/// @brief Determine the range of addresses that are reusable
/// (Reusable -> not occupied by the current program, not
/// occupied by the wifi-settings file, and within the current partition, if any.)
/// @param[out] r Flash memory range
void wifi_settings_range_get_reusable(
    wifi_settings_flash_range_t* r);

/// @brief Determine the range of addresses used by the wifi-settings file
/// @param[out] r Flash memory range
/// @details This function has a weak symbol, allowing it to be reimplemented
/// by applications in order to place the file at any Flash location,
/// including a location that is determined dynamically. A different
/// static location can also be set at build time with
/// -DWIFI_SETTINGS_FILE_ADDRESS=0x...
void wifi_settings_range_get_wifi_settings_file(
    wifi_settings_flash_range_t* r);

/// @brief Determine the range of addresses used by the current program
/// @param[out] r Flash memory range
void wifi_settings_range_get_program(
    wifi_settings_flash_range_t* r);

/// @brief Determine the range of addresses used by the current partition
/// @param[out] r Flash memory range
void wifi_settings_range_get_partition(
    wifi_settings_flash_range_t* r);

/// @brief Translate Flash range to logical range
/// @param[in] fr Flash memory range
/// @param[out] lr Logical memory range
void wifi_settings_range_translate_to_logical(
    const wifi_settings_flash_range_t* fr,
    wifi_settings_logical_range_t* lr);

/// @brief Align Flash range to sector boundary and size:
/// no effect if they are already aligned
/// @param[inout] fr Flash memory range (possibly unaligned)
void wifi_settings_range_align_to_sector(
    wifi_settings_flash_range_t* fr);

/// @brief Translate logical range to Flash range if possible.
/// Not possible if the logical range is outside of an accessible area of Flash:
/// (1) ROM/RAM/other non-Flash addresses, (2) outside of the current partition,
/// (3) outside of both XIP_BASE and XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE regions.
/// @param[in] lr Logical memory range
/// @param[out] fr Flash memory range
/// @return true if translation was possible
bool wifi_settings_range_translate_to_flash(
    const wifi_settings_logical_range_t* lr,
    wifi_settings_flash_range_t* fr);

#endif 
