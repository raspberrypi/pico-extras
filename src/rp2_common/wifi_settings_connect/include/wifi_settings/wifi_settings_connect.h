/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This header file declares functions used to initialise and connect to WiFi
 * with pico-wifi-settings.
 *
 */

#ifndef _WIFI_SETTINGS_CONNECT_H_
#define _WIFI_SETTINGS_CONNECT_H_

#include <stdbool.h>

// These settings are fixed by WPA-PSK standards
#define WIFI_SSID_SIZE      33      // including '\0' character
#define WIFI_BSSID_SIZE     6       // size of a MAC address
#define WIFI_PASSWORD_SIZE  65      // including '\0' character

/// @brief Initialise wifi_settings module
/// @return 0 on success, or an error code from cyw43_arch_init
int wifi_settings_init();

/// @brief Deinitialise wifi_settings module
void wifi_settings_deinit();

/// @brief Connect to WiFi if possible, using the settings in Flash.
/// The actual connection may take some time to be established, and
/// may not be possible. Call wifi_settings_is_connected() to see if
/// the connection is ready.
void wifi_settings_connect();

/// @brief Disconnect from WiFi immediately.
void wifi_settings_disconnect();

/// @brief Determine if connection is ready.
/// @return true if ready
bool wifi_settings_is_connected();

/// @brief Determine if the WiFi settings are empty - if the
/// file is empty, wifi_settings will be unable to connect. See README.md
/// for instructions on how to provide settings.
/// @return true if empty (no known SSIDs or BSSIDs)
bool wifi_settings_has_no_wifi_details();

/// @brief Get a report on the current connection status
/// @param[inout] text Text buffer for the report
/// @param[in] text_size Available space in the buffer (bytes)
/// @return Return code from snprintf when formatting
int wifi_settings_get_connect_status_text(char* text, int text_size);

/// @brief Get a report on the network hardware (cyw43) status (e.g. signal strength)
/// @param[inout] text Text buffer for the report
/// @param[in] text_size Available space in the buffer (bytes)
/// @return Return code from snprintf when formatting
int wifi_settings_get_hw_status_text(char* text, int text_size);

/// @brief Get a report on the IP stack status (e.g. IP address)
/// @param[inout] text Text buffer for the report
/// @param[in] text_size Available space in the buffer (bytes)
/// @return Return code from snprintf when formatting
int wifi_settings_get_ip_status_text(char* text, int text_size);

/// @brief Get the status of a connection attempt to
/// an SSID as a static string, e.g. SUCCESS, NOT_FOUND. "" is returned
/// if the SSID index is not known.
/// @param[in] ssid_index Index matching the ssid number.
/// @return Static string containing the SSID status.
const char* wifi_settings_get_ssid_status(int ssid_index);


#endif
