/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This header file is intended to be internal and should not be included directly by applications.
 *
 */

#ifndef WIFI_SETTINGS_CONNECT_INTERNAL_H
#define WIFI_SETTINGS_CONNECT_INTERNAL_H

#ifndef WIFI_SETTINGS_CONNECT_C
#error "This is an internal header intended only for use by wifi_settings_connect.c and its unit tests"
#else

#include "wifi_settings_configuration.h"

#include "pico/async_context.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#include "pico/cyw43_arch.h"

enum wifi_connect_state_t {
    UNINITIALISED = 0,          // cyw43 hardware was not started
    INITIALISATION_ERROR,       // initialisation failed (see hw_error_code)
    STORAGE_EMPTY_ERROR,        // no WiFi details are known
    DISCONNECTED,               // call wifi_settings_connect() to connect
    TRY_TO_CONNECT,             // connection process begun
    SCANNING,                   // scan running
    CONNECTING,                 // connection running
    CONNECTED_IP,               // connection is ready for use
};

enum ssid_scan_info_t {
    NOT_FOUND = 0,              // this SSID was not found
    FOUND,                      // this SSID was found by the most recent scan
    ATTEMPT,                    // we attempted to connect to this SSID
    FAILED,                     // ... but it failed with an error
    TIMEOUT,                    // ... but it failed with a timeout
    BADAUTH,                    // ... but the password is wrong
    SUCCESS,                    // ... and it worked
    LOST,                       // we connected to this SSID but the connection dropped
};

#define IPV4_ADDRESS_SIZE   16      // "xxx.xxx.xxx.xxx\0"
#define KEY_SIZE            10      // e.g. "bssid0"

struct wifi_state_t {
    enum wifi_connect_state_t   cstate;
    enum ssid_scan_info_t       ssid_scan_info[NUM_SSIDS + 1];
    struct netif*               netif;
    cyw43_t*                    cyw43;
    uint                        selected_ssid_index;
    int                         hw_error_code;
    absolute_time_t             connect_timeout_time;
    absolute_time_t             scan_holdoff_time;
    async_context_t*            context;
    async_at_time_worker_t      periodic_worker;
};

#endif
#endif
