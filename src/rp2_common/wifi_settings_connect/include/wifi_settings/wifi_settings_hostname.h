/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Hostname for wifi-settings. The hostname can be specified in
 * the WiFi settings file as "name=<xxx>".
 */

#ifndef _WIFI_SETTINGS_HOSTNAME_H_
#define _WIFI_SETTINGS_HOSTNAME_H_

#define MAX_HOSTNAME_SIZE       64
#define BOARD_ID_SIZE           8

/// @brief Return a pointer to the hostname (which is a global variable)
/// @return pointer to the hostname
const char* wifi_settings_get_hostname();

/// @brief Return a pointer to the board ID in hex format (this is a global variable)
/// @return pointer to the board ID in hex format
const char* wifi_settings_get_board_id_hex();

/// @brief Load the hostname from the wifi-settings file
void wifi_settings_set_hostname();

#endif
