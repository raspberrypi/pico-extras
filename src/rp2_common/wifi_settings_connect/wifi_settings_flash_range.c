/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This file defines functions used to check and translate address ranges.
 *
 */

#include "wifi_settings/wifi_settings_configuration.h"
#include "wifi_settings/wifi_settings_flash_range.h"

#include "pico/platform.h"
#include "hardware/regs/addressmap.h"
#ifdef XIP_QMI_BASE
#include "hardware/structs/qmi.h"
#endif

// Calculate end address for a range
static uint32_t get_end_address(const wifi_settings_flash_range_t* r) {
    return r->start_address + r->size;
}

// A valid range contains at least 1 byte, and the end address is greater than
// the start address. (So, a valid range never wraps over UINT_MAX.)
static uint32_t is_valid(const wifi_settings_flash_range_t* r) {
    return get_end_address(r) > r->start_address;
}

// Determine if inner is within outer (or exactly the same as outer)
bool wifi_settings_range_is_contained(
    const wifi_settings_flash_range_t* inner,
    const wifi_settings_flash_range_t* outer) {

    if ((!is_valid(inner)) || (!is_valid(outer))) {
        // At least one of the ranges is not valid (must contain at least 1 byte
        // and not wrap over UINT_MAX.)
        return false;
    }
    if (inner->start_address < outer->start_address) {
        // inner starts before outer
        return false;
    }
    if (get_end_address(inner) > get_end_address(outer)) {
        // inner ends after outer
        return false;
    }
    // inner is no larger than outer
    return true;
}

// Detect if a Flash memory range intersects with another
bool wifi_settings_range_has_overlap(
    const wifi_settings_flash_range_t* fr1,
    const wifi_settings_flash_range_t* fr2)
{
    if ((!is_valid(fr1)) || (!is_valid(fr2))) {
        // Invalid ranges are considered non-overlapping
        return false;
    }
    if (fr1->start_address >= get_end_address(fr2)) {
        return false; // gap: fr1 is after fr2
    }
    if (fr2->start_address >= get_end_address(fr1)) {
        return false; // gap: fr2 is after fr1
    }
    // no gap: fr1 and fr2 must overlap
    return true;
}

// Determine the range of addresses that are in Flash
void wifi_settings_range_get_all(wifi_settings_flash_range_t* r) {
    r->start_address = 0;
    r->size = PICO_FLASH_SIZE_BYTES;
}


extern char __flash_binary_end;

// Determine the range of Flash addresses used by the current program
void wifi_settings_range_get_program(wifi_settings_flash_range_t* r) {
    // Initial setup: assume the program is not in Flash
    r->start_address = 0;
    r->size = 0;

    // If the program is in Flash, determine the size
    const uintptr_t end_of_program = ((uintptr_t) &__flash_binary_end);
    if ((end_of_program > XIP_BASE)
    && (end_of_program <= (XIP_BASE + PICO_FLASH_SIZE_BYTES))) {
        // get start of partition
        wifi_settings_range_get_partition(r);
        // get program size
        r->size = end_of_program - XIP_BASE;
    }
}

// Determine the range of Flash addresses for the current partition
void wifi_settings_range_get_partition(wifi_settings_flash_range_t* r) {
#ifdef XIP_QMI_BASE
    // Partition base and size is in the ATRANS0 register
    const uint32_t atrans0 = *((io_ro_32*)(XIP_QMI_BASE + QMI_ATRANS0_OFFSET));
    const uint32_t base = atrans0 & 0xfff;
    const uint32_t size = (atrans0 >> 16) & 0x7ff;
    r->start_address = base * FLASH_SECTOR_SIZE;
    r->size = size * FLASH_SECTOR_SIZE;
#else
    // Partition is the whole of Flash
    r->start_address = 0;
    r->size = PICO_FLASH_SIZE_BYTES;
#endif
}

// Determine the range of addresses used by the wifi-settings file
// This function can be reimplemented in order to set the file location dynamically;
// this default version uses values from wifi_settings_configuration.h which are
// guaranteed to be valid because of static assertions in the header
__weak void wifi_settings_range_get_wifi_settings_file(wifi_settings_flash_range_t* r) {
    r->start_address = WIFI_SETTINGS_FILE_ADDRESS;
    r->size = WIFI_SETTINGS_FILE_SIZE;
}

// Determine the range of addresses that are reusable
// They are between the end of the program and either the start of the
// wifi-settings file, or the end of the partition, whichever comes first
void wifi_settings_range_get_reusable(wifi_settings_flash_range_t* r) {
    wifi_settings_flash_range_t program_range;
    wifi_settings_flash_range_t partition_range;
    wifi_settings_flash_range_t wifi_settings_file_range;

    wifi_settings_range_get_program(&program_range);
    wifi_settings_range_get_partition(&partition_range);
    wifi_settings_range_get_wifi_settings_file(&wifi_settings_file_range);

    // round up the program size
    wifi_settings_range_align_to_sector(&program_range);

    const uint32_t end_of_partition = get_end_address(&partition_range);
    const uint32_t start_of_settings_file = wifi_settings_file_range.start_address;
    const uint32_t end_of_reusable_space =
        (end_of_partition < start_of_settings_file) ? end_of_partition : start_of_settings_file;

    r->start_address = get_end_address(&program_range);
    if (end_of_reusable_space <= r->start_address) {
        // There is no reusable space
        r->start_address = 0;
        r->size = 0;
    } else {
        r->size = end_of_reusable_space - r->start_address;
    }
}

// Translate Flash range to logical range
void wifi_settings_range_translate_to_logical(
        const wifi_settings_flash_range_t* fr,
        wifi_settings_logical_range_t* lr) {

#ifdef XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE
    // if the Flash memory might use address translation, use a non-translated address (used for Pico 2)
    lr->start_address = (void*) ((uintptr_t) (fr->start_address + XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE));
#else
    // XIP_BASE represents flash address 0
    lr->start_address = (void*) ((uintptr_t) (fr->start_address + XIP_BASE));
#endif
    lr->size = fr->size;
}

void wifi_settings_range_align_to_sector(wifi_settings_flash_range_t* fr) {
    // Start address is rounded down (no effect if already aligned)
    fr->start_address &= ~(FLASH_SECTOR_SIZE - 1);
    // Size is rounded up (no effect if already aligned)
    if ((fr->size & (FLASH_SECTOR_SIZE - 1)) != 0) {
        fr->size |= FLASH_SECTOR_SIZE - 1;
        fr->size++;
    }
}

// Translate logical range to Flash range, if possible
bool wifi_settings_range_translate_to_flash(
        const wifi_settings_logical_range_t* lr,
        wifi_settings_flash_range_t* fr) {

    const uintptr_t start_address = (uintptr_t) lr->start_address;
    const uintptr_t end_address = start_address + lr->size;

    fr->start_address = 0;
    fr->size = lr->size;

    if (end_address < start_address) {
        return false; // Range is too large (overflow)
    }

    if ((start_address >= XIP_BASE)
#ifdef XIP_NOALLOC_BASE
    && (end_address < XIP_NOALLOC_BASE) // Pico 1: end of main region of Flash
#endif
#ifdef XIP_END
    && (end_address < XIP_END) // Pico 2: end of main region of Flash
#endif
    && (end_address < SRAM_BASE)) { // Fallback: end of all Flash addresses

        // Address is within the part of Flash that can use address translation;
        // apply address translation based on the partition size if relevant.
        wifi_settings_flash_range_t pr;
        wifi_settings_range_get_partition(&pr);
        fr->start_address = start_address + pr.start_address - XIP_BASE;
        // If the resulting range is still contained in the partition, then the
        // translation was successful
        return wifi_settings_range_is_contained(fr, &pr);
    }

#ifdef XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE
    if ((start_address >= XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE)
    && (end_address < SRAM_BASE)) {
        // Pico 2: Address is within the untranslated region of Flash
        wifi_settings_flash_range_t ar;
        wifi_settings_range_get_all(&ar);
        fr->start_address = start_address - XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE;
        // If the resulting range is still contained in Flash, then the
        // translation was successful
        return wifi_settings_range_is_contained(fr, &ar);
    }
#endif

    // Not in Flash, or not in an area of Flash that can be supported
    return false;
}

