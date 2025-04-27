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


#if defined(FLASH_ADDRESS_OF_WIFI_SETTINGS_FILE)
// Flash address of wifi-settings file already defined
#elif PICO_RP2040
// Flash address of wifi-settings file on Pico 1 W
#define FLASH_ADDRESS_OF_WIFI_SETTINGS_FILE         0x001ff000
// Note: Flash addresses have 0 = start of Flash.
// Note: The CPU's address for the wifi-settings file is 0x101ff000 on Pico 1.

#elif PICO_RP2350
// Flash address of wifi-settings file on Pico 2 W
#define FLASH_ADDRESS_OF_WIFI_SETTINGS_FILE         0x003fe000
// Note: Flash addresses have 0 = start of Flash.
// Note: avoid final sector due to RP2350-E10 bug
// Note: The CPU's address for the wifi-settings file is 0x103fe000 on Pico 2
// assuming that there is no translation (e.g. partitioning).

#else
#error "Unknown Pico model - please set FLASH_ADDRESS_OF_WIFI_SETTINGS_FILE appropriately"
#endif

// Size of wifi-settings file (bytes, must be a whole number of Flash sectors)
#ifndef WIFI_SETTINGS_FILE_SIZE
#define WIFI_SETTINGS_FILE_SIZE         0x1000
#endif

// Minimum time between initialisation and the first scan (milliseconds).
#ifndef INITIAL_SETUP_TIME_MS
#define INITIAL_SETUP_TIME_MS           1000
#endif

// Maximum time allowed between calling cyw43_wifi_join and getting an IP address (milliseconds).
// If this timeout expires, wifi_settings will try a different hotspot or rescan. The attempt to
// join a hotspot can fail sooner than this, e.g. if the password is incorrect or the hotspot vanishes.
#ifndef CONNECT_TIMEOUT_TIME_MS
#define CONNECT_TIMEOUT_TIME_MS         30000
#endif

// Minimum time between scans (milliseconds). If a scan fails to find any known hotspot,
// wifi_settings will always wait at least this long before retry.
#ifndef REPEAT_SCAN_TIME_MS
#define REPEAT_SCAN_TIME_MS             3000
#endif

// Minimum time between calls to the periodic function, wifi_settings_periodic_callback,
// which will initiate scans and connections if necessary (milliseconds).
#ifndef PERIODIC_TIME_MS
#define PERIODIC_TIME_MS                1000
#endif

// Maximum number of SSIDs that can be supported.
#ifndef NUM_SSIDS
#define NUM_SSIDS                       16
#endif

#endif
