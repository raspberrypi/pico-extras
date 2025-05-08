/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This header file contains default values for timeouts,
 * addresses and limits within the pico-wifi-settings library.
 */

#ifndef WIFI_SETTINGS_CONFIGURATION_H
#define WIFI_SETTINGS_CONFIGURATION_H

#include "hardware/flash.h"

#if defined(WIFI_SETTINGS_FILE_ADDRESS) && (WIFI_SETTINGS_FILE_ADDRESS == 0)
// If WIFI_SETTINGS_FILE_ADDRESS == 0, then use the default start location
#undef WIFI_SETTINGS_FILE_ADDRESS
#endif

#if !defined(WIFI_SETTINGS_FILE_ADDRESS)
// The default start location of the wifi-settings file is 16kb before the end of Flash:
#define WIFI_SETTINGS_FILE_ADDRESS         (PICO_FLASH_SIZE_BYTES - 0x4000)
//
// The CPU's address for this Flash location is:
//         0x101fc000 on Pico 1 W
//         0x103fc000 on Pico 2 W
//    and may have other values on other RP2040/RP2350 boards.
//
// This location is chosen because the final three 4kb sectors of Flash are
// already assigned a function by the Pico SDK. The Bluetooth library uses
// two 4kb sectors for storage of devices that have been paired by Bluetooth.
// The final 4kb sector is used for a workaround for the RP2350-E10 bug - this
// sector may be erased when copying a UF2 file to a Pico 2 via drag-and-drop.
// Therefore, these three sectors are avoided.
//
// If you wish to store the wifi-settings file at a specific address you can
// do so by setting -DWIFI_SETTINGS_FILE_ADDRESS=0x.... when running
// cmake. This should be an address relative to the start of Flash, and
// should be a multiple of WIFI_SETTINGS_FILE_SIZE.
//
// Versions of pico-wifi-settings before 0.2.0 used 0x1ff000 for Pico 1 and
// 0x3fe000 for Pico 2.
#endif

// Size of wifi-settings file in bytes
// This must be a whole number of Flash sectors.
// The setup app, documentation and examples all assume 0x1000 bytes, and that
// is the recommended value, but any positive multiple of the Flash sector size
// can be used.
#ifndef WIFI_SETTINGS_FILE_SIZE
#define WIFI_SETTINGS_FILE_SIZE         (1 * FLASH_SECTOR_SIZE)   // (0x1000 bytes)
#endif

// Minimum time between initialisation and the first scan (milliseconds).
#ifndef INITIAL_SETUP_TIME_MS
#define INITIAL_SETUP_TIME_MS           1000
#endif

// Maximum time allowed between calling cyw43_wifi_join and getting an
// IP address (milliseconds). If this timeout expires, wifi_settings will
// try a different hotspot or rescan. The attempt to join a hotspot can fail
// sooner than this, e.g. if the password is incorrect or the hotspot vanishes.
#ifndef CONNECT_TIMEOUT_TIME_MS
#define CONNECT_TIMEOUT_TIME_MS         30000
#endif

// Minimum time between scans (milliseconds). If a scan fails to find any
// known hotspot, wifi_settings will always wait at least this long before
// retry.
#ifndef REPEAT_SCAN_TIME_MS
#define REPEAT_SCAN_TIME_MS             3000
#endif

// Minimum time between calls to the periodic function,
// wifi_settings_periodic_callback,
// which will initiate scans and connections if necessary (milliseconds).
#ifndef PERIODIC_TIME_MS
#define PERIODIC_TIME_MS                1000
#endif

// Maximum number of SSIDs that can be supported. This determines the size
// of the g_wifi_state.ssid_scan_info array. You can set this maximum
// to larger values if you wish, at the cost of some additional memory
// usage (1 byte per SSID), but the setup app assumes this maximum.
#ifndef MAX_NUM_SSIDS
#define MAX_NUM_SSIDS                   100
#endif

// Validation for wifi-settings file address and size
#ifdef static_assert
static_assert((WIFI_SETTINGS_FILE_ADDRESS + WIFI_SETTINGS_FILE_SIZE) <= PICO_FLASH_SIZE_BYTES);
static_assert(WIFI_SETTINGS_FILE_ADDRESS > 0);
static_assert(WIFI_SETTINGS_FILE_SIZE > 0);
static_assert((WIFI_SETTINGS_FILE_SIZE % FLASH_SECTOR_SIZE) == 0);
static_assert((WIFI_SETTINGS_FILE_ADDRESS % WIFI_SETTINGS_FILE_SIZE) == 0);
#endif

#endif
