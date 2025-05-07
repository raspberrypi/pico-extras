/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This pico-wifi-settings module manages the WiFi connection
 * by calling cyw43 and LwIP functions.
 *
 */


#define WIFI_SETTINGS_CONNECT_C
#include "wifi_settings/wifi_settings_configuration.h"
#include "wifi_settings/wifi_settings_connect.h"
#include "wifi_settings/wifi_settings_connect_internal.h"
#include "wifi_settings/wifi_settings_flash_storage.h"
#include "wifi_settings/wifi_settings_hostname.h"

#ifdef ENABLE_REMOTE_UPDATE
#include "wifi_settings/wifi_settings_remote.h"
#endif

#include "pico/binary_info.h"
#include "pico/error.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>


struct wifi_state_t g_wifi_state;

enum ssid_type_t {
    NONE = 0,
    BSSID,
    SSID,
};

static enum ssid_type_t fetch_ssid(uint ssid_index, char* ssid, uint8_t* bssid);


bool wifi_settings_has_no_wifi_details() {
    char ssid[WIFI_SSID_SIZE];
    uint8_t bssid[WIFI_BSSID_SIZE];
    return fetch_ssid(1, ssid, bssid) == NONE;
}

int wifi_settings_get_connect_status_text(char* text, int text_size) {
    char ssid[WIFI_SSID_SIZE];
    uint8_t bssid[WIFI_BSSID_SIZE];
    enum ssid_type_t ssid_type;

    switch(g_wifi_state.cstate) {
        case TRY_TO_CONNECT:
            return snprintf(text, text_size, "WiFi did not find any known hotspot yet");
        case SCANNING:
            return snprintf(text, text_size, "WiFi is scanning for hotspots");
        case CONNECTING:
            ssid_type = fetch_ssid(g_wifi_state.selected_ssid_index, ssid, bssid);
            return snprintf(text, text_size,
                "WiFi is connecting to %sssid%u=%s",
                (ssid_type == BSSID) ? "b" : "",
                g_wifi_state.selected_ssid_index,
                ssid);
        case CONNECTED_IP:
            ssid_type = fetch_ssid(g_wifi_state.selected_ssid_index, ssid, bssid);
            return snprintf(text, text_size,
                "WiFi is connected to %sssid%u=%s",
                (ssid_type == BSSID) ? "b" : "",
                g_wifi_state.selected_ssid_index,
                ssid);
        case DISCONNECTED:
            return snprintf(text, text_size, "WiFi is disconnected");
        case UNINITIALISED:
            return snprintf(text, text_size, "WiFi uninitialised");
        case INITIALISATION_ERROR:
            return snprintf(text, text_size, "WiFi init error: %d",
                    g_wifi_state.hw_error_code);
        case STORAGE_EMPTY_ERROR:
            return snprintf(text, text_size,
                "No WiFi details have been stored - unable to connect");
        default:
            break;
    }
    return snprintf(text, text_size, "WiFi status is unknown (%d)", (int) g_wifi_state.cstate);
}

int wifi_settings_get_hw_status_text(char* text, int text_size) {
    if (!g_wifi_state.cyw43) {
        text[0] = '\0';
        return 0;
    }

    const char* hw_status_text = "?";
    switch (cyw43_wifi_link_status(g_wifi_state.cyw43, CYW43_ITF_STA)) {
        case CYW43_LINK_DOWN:       hw_status_text = "DOWN"; break;
        case CYW43_LINK_JOIN:       hw_status_text = "JOIN"; break;
        case CYW43_LINK_NOIP:       hw_status_text = "NOIP"; break;
        case CYW43_LINK_UP:         hw_status_text = "UP"; break;
        case CYW43_LINK_FAIL:       hw_status_text = "FAIL"; break;
        case CYW43_LINK_NONET:      hw_status_text = "NONET"; break;
        case CYW43_LINK_BADAUTH:    hw_status_text = "BADAUTH"; break;
        default: break;
    }
    int32_t rssi = -1;
    cyw43_wifi_get_rssi(g_wifi_state.cyw43, &rssi);
    return snprintf(text, text_size,
        "cyw43_wifi_link_status = CYW43_LINK_%s scan_active = %s rssi = %d",
        hw_status_text,
        cyw43_wifi_scan_active(g_wifi_state.cyw43) ? "True" : "False",
        (int) rssi);
}

int wifi_settings_get_ip_status_text(char* text, int text_size) {
    if ((!g_wifi_state.netif)
    || (!netif_is_link_up(g_wifi_state.netif))) {
        text[0] = '\0';
        return 0;
    }
    char addr_buf1[IPV4_ADDRESS_SIZE];
    char addr_buf2[IPV4_ADDRESS_SIZE];
    char addr_buf3[IPV4_ADDRESS_SIZE];
    return snprintf(text, text_size,
        "IPv4 address = %s netmask = %s gateway = %s",
        ip4addr_ntoa_r(netif_ip4_addr(g_wifi_state.netif), addr_buf1, IPV4_ADDRESS_SIZE),
        ip4addr_ntoa_r(netif_ip4_netmask(g_wifi_state.netif), addr_buf2, IPV4_ADDRESS_SIZE),
        ip4addr_ntoa_r(netif_ip4_gw(g_wifi_state.netif), addr_buf3, IPV4_ADDRESS_SIZE));
}

int wifi_settings_get_ip(char* text, int text_size) {
    if ((!g_wifi_state.netif)
    || (!netif_is_link_up(g_wifi_state.netif))) {
        // Not connected - return empty string
        text[0] = '\0';
        return 0;
    }
    char addr_buf[IPV4_ADDRESS_SIZE];
    return snprintf(text, text_size,
        "%s",
        ip4addr_ntoa_r(netif_ip4_addr(g_wifi_state.netif), addr_buf, IPV4_ADDRESS_SIZE));
}

int wifi_settings_get_ssid(char* text, int text_size) {
    char ssid[WIFI_SSID_SIZE];
    uint8_t bssid[WIFI_BSSID_SIZE];

    switch(g_wifi_state.cstate) {
        case CONNECTING:
        case CONNECTED_IP:
            (void) fetch_ssid(g_wifi_state.selected_ssid_index, ssid, bssid);
            // The text buffer will contain '?' if the SSID is unknown (e.g. if
            // the wifi-settings file was updated to remove the SSID while connected).
            return snprintf(text, text_size, "%s", ssid);
        default:
            // Not connected - return empty string
            text[0] = '\0';
            return 0;
    }
}

const char* wifi_settings_get_ssid_status(int ssid_index) {
    if ((ssid_index >= 1) && (ssid_index <= MAX_NUM_SSIDS)) {
        switch (g_wifi_state.ssid_scan_info[ssid_index]) {
            case NOT_FOUND: return "NOT FOUND"; break;
            case FOUND:     return "FOUND"; break;
            case ATTEMPT:   return "ATTEMPT"; break;
            case SUCCESS:   return "SUCCESS"; break;
            case FAILED:    return "FAILED"; break;
            case TIMEOUT:   return "TIMEOUT"; break;
            case BADAUTH:   return "BADAUTH"; break;
            case LOST:      return "LOST"; break;
            default: break;
        }
    }
    return "";
}

static bool wifi_is_connected() {
    if (g_wifi_state.netif) {
        return netif_is_link_up(g_wifi_state.netif);
    } else {
        return false;
    }
}

static bool convert_string_to_bssid(const char* text, uint text_size, uint8_t* bssid) {
    // A BSSID is specified in the file as bssid1=01:23:45:67:89:ab
    // note 1 - ':' separators
    // note 2 - exactly 17 bytes
    memset(bssid, 0, WIFI_BSSID_SIZE);
    if (text_size != ((WIFI_BSSID_SIZE * 3) - 1)) {
        // Malformed BSSID - not exactly 17 bytes
        return false;
    }
    for (uint i = 0; i < (WIFI_BSSID_SIZE - 1); i++) {
        if (text[(i * 3) + 2] != ':') {
            // Malformed BSSID - not ':' separator
            return false;
        }
    }
    for (uint i = 0; i < WIFI_BSSID_SIZE; i++) {
        char copy[3];
        char* check = NULL;
        copy[0] = text[(i * 3) + 0];
        copy[1] = text[(i * 3) + 1];
        copy[2] = '\0';
        bssid[i] = (uint8_t) strtol(copy, &check, 16);
        if (check != &copy[2]) {
            // Malformed BSSID - not a hex number
            return false;
        }
    }
    // valid BSSID
    return true;
}

static enum ssid_type_t fetch_ssid(uint ssid_index, char* ssid, uint8_t* bssid) {
    // Generate search key
    char key[KEY_SIZE];
    snprintf(key, sizeof(key), "bssid%u", ssid_index);

    uint ssid_size = WIFI_SSID_SIZE;
    memset(bssid, 0, WIFI_BSSID_SIZE);

    // A BSSID is specified in the file as bssid1=01:23:45:67:89:ab
    if (wifi_settings_get_value_for_key(key, ssid, &ssid_size)) {
        ssid[ssid_size] = '\0';
        if (convert_string_to_bssid(ssid, ssid_size, bssid)) {
            return BSSID;
        }
    }

    // An SSID is specified in the file as ssid1=MyHotspotName
    // and must match exactly; SSIDs cannot contain characters recognised
    // as end of line or end of file (\r \n \xff \x00 \x1a)
    if (wifi_settings_get_value_for_key(&key[1], ssid, &ssid_size)) {
        ssid[ssid_size] = '\0';
        return SSID;
    }
    // Undefined SSID and BSSID
    ssid[0] = '?';
    ssid[1] = '\0';
    return NONE;
}

static int wifi_scan_callback(void* unused, const cyw43_ev_scan_result_t* scan_result) {
    // Is this SSID known? Iterate through the file to see if there is a record of it.
    for (uint ssid_index = 1; ssid_index <= MAX_NUM_SSIDS; ssid_index++) {
        // Skip SSIDs that we already saw
        if (g_wifi_state.ssid_scan_info[ssid_index] != NOT_FOUND) {
            continue;
        }

        // Check file entries for bssid<ssid_index> and ssid<ssid_index>
        char ssid[WIFI_SSID_SIZE];
        uint8_t bssid[WIFI_BSSID_SIZE];
        enum ssid_type_t ssid_type = fetch_ssid(ssid_index, ssid, bssid);
        switch (ssid_type) {
            case BSSID:
                if (memcmp(bssid, scan_result->bssid, WIFI_BSSID_SIZE) == 0) {
                    // BSSID match
                    g_wifi_state.ssid_scan_info[ssid_index] = FOUND;
                }
                break;
            case SSID:
                if ((strlen(ssid) == (uint) scan_result->ssid_len)
                && (memcmp(scan_result->ssid, ssid, (uint) scan_result->ssid_len) == 0)) {
                    // SSID match
                    g_wifi_state.ssid_scan_info[ssid_index] = FOUND;
                }
                break;
            case NONE:
                // ssid<n> doesn't exist, so ssid<n+1>, ssid<n+2> etc. won't be checked
                return 0;
        }
    }
    // No more entries to try
    return 0;
}

static void ensure_disconnected() {
    cyw43_wifi_leave(g_wifi_state.cyw43, CYW43_ITF_STA);
    g_wifi_state.netif = NULL;
}

static void begin_connecting() {
    // This function is called after a scan, to begin connecting to a new hotspot.
    // It looks at the results of the scan and previous connections, via ssid_scan_info.
    ensure_disconnected();

    // Which hotspot to connect to?
    g_wifi_state.selected_ssid_index = 0;
    for (uint ssid_index = 1; ssid_index <= MAX_NUM_SSIDS; ssid_index++) {
        if (g_wifi_state.ssid_scan_info[ssid_index] == FOUND) {
            g_wifi_state.selected_ssid_index = ssid_index;
            break;
        }
    }

    if (g_wifi_state.selected_ssid_index == 0) {
        // There are no available hotspots to connect to, either because the scan
        // didn't find anything, or everything is FAILED, TIMEOUT, BADAUTH or LOST.
        // In this case we should scan again.
        g_wifi_state.cstate = TRY_TO_CONNECT;
        return;
    }

    // Begin connecting
    g_wifi_state.ssid_scan_info[g_wifi_state.selected_ssid_index] = ATTEMPT;
    g_wifi_state.connect_timeout_time = make_timeout_time_ms(CONNECT_TIMEOUT_TIME_MS);
    g_wifi_state.cstate = CONNECTING;

    // Get the password
    char key[KEY_SIZE];
    snprintf(key, sizeof(key), "pass%u", g_wifi_state.selected_ssid_index);
    char password[WIFI_PASSWORD_SIZE];
    uint password_size = sizeof(password) - 1;
    uint32_t auth_type = CYW43_AUTH_WPA2_AES_PSK;
    if (!wifi_settings_get_value_for_key(key, password, &password_size)) {
        // No password specified (open WiFi)
        password_size = 0;
        auth_type = CYW43_AUTH_OPEN;
    }
    password[password_size] = '\0';

    // Get the BSSID or SSID
    char ssid[WIFI_SSID_SIZE];
    uint8_t bssid[WIFI_BSSID_SIZE];
    enum ssid_type_t ssid_type = fetch_ssid(g_wifi_state.selected_ssid_index, ssid, bssid);
    if (ssid_type == NONE) {
        // No valid SSID or BSSID - this could happen if the storage was updated
        // between scanning and connecting. Force a rescan
        g_wifi_state.selected_ssid_index = 0;
        g_wifi_state.cstate = TRY_TO_CONNECT;
        return;
    }

    // Begin connection
    if (ssid_type == BSSID) {
        g_wifi_state.hw_error_code = cyw43_wifi_join(g_wifi_state.cyw43,
                0, // size_t ssid_len
                NULL, // const uint8_t *ssid
                password_size, // size_t key_len
                (const uint8_t *) password, // const uint8_t *key
                auth_type, // uint32_t auth_type
                bssid, // const uint8_t *bssid
                CYW43_CHANNEL_NONE); // uint32_t channel
    } else {
        g_wifi_state.hw_error_code = cyw43_wifi_join(g_wifi_state.cyw43,
                strlen(ssid), // size_t ssid_len
                (const uint8_t *) ssid, // const uint8_t *ssid
                password_size, // size_t key_len
                (const uint8_t *) password, // const uint8_t *key
                auth_type, // uint32_t auth_type
                NULL, // const uint8_t *bssid
                CYW43_CHANNEL_NONE); // uint32_t channel
    }
}

static void give_up_connecting(enum ssid_scan_info_t info) {
    // Mark the selected SSID as bad in some way (e.g. BADAUTH, TIMEOUT)
    // so that it won't be tried again. Go back to the SCANNING state.
    g_wifi_state.ssid_scan_info[g_wifi_state.selected_ssid_index] = info;
    g_wifi_state.cstate = SCANNING;
}

static bool has_valid_address() {
    char address_buf[IPV4_ADDRESS_SIZE];
    if (g_wifi_state.netif) {
        const char* address = ip4addr_ntoa_r(netif_ip4_addr(g_wifi_state.netif),
                            address_buf, IPV4_ADDRESS_SIZE);
        if ((address[0] != '\0') && (strcmp("0.0.0.0", address) != 0)) {
            return true;
        }
    }
    return false;
}

static void begin_new_scan() {
    // Begin a scan. We will reset everything we know about hotspots first.
    for (uint ssid_index = 1; ssid_index <= MAX_NUM_SSIDS; ssid_index++) {
        g_wifi_state.ssid_scan_info[ssid_index] = NOT_FOUND;
    }
    // Start the scan
    cyw43_wifi_scan_options_t opts;
    memset(&opts, 0, sizeof(opts));
    g_wifi_state.hw_error_code = cyw43_wifi_scan(g_wifi_state.cyw43, &opts, NULL, wifi_scan_callback);
    g_wifi_state.cstate = SCANNING;
    g_wifi_state.scan_holdoff_time = make_timeout_time_ms(REPEAT_SCAN_TIME_MS);
}

static void wifi_settings_periodic_callback(async_context_t* unused1, async_at_time_worker_t* unused2) {
    switch (g_wifi_state.cstate) {
        case TRY_TO_CONNECT:
            // In this state, we are not connected, and we are waiting for a holdoff time
            // before beginning a scan for available hotspots. If a scan is already running
            // (e.g. due to disconnecting during a scan) we wait for it to finish.
            ensure_disconnected();
            if (wifi_settings_has_no_wifi_details()) {
                // This is reached if the storage file contains no SSIDs.
                g_wifi_state.cstate = STORAGE_EMPTY_ERROR;
            } else if (time_reached(g_wifi_state.scan_holdoff_time) && !cyw43_wifi_scan_active(g_wifi_state.cyw43)) {
                begin_new_scan();
            }
            break;
        case SCANNING:
            // In this state, we are waiting for a hotspot scan to complete.
            // If it already completed, and we have some results, we can go directly to CONNECTING.
            if (!cyw43_wifi_scan_active(g_wifi_state.cyw43)) {
                begin_connecting();
            }
            break;
        case CONNECTING:
            // In this state, we are joining a WiFi hotspot, having found at least one
            // possibility during the scan.
            switch (cyw43_wifi_link_status(g_wifi_state.cyw43, CYW43_ITF_STA)) {
                case CYW43_LINK_DOWN:
                case CYW43_LINK_FAIL:
                case CYW43_LINK_NONET:
                    // Connection failed - this hotspot must have disappeared
                    give_up_connecting(FAILED);
                    break;
                case CYW43_LINK_BADAUTH:
                    // Connection failed because the password is incorrect
                    give_up_connecting(BADAUTH);
                    break;
                case CYW43_LINK_JOIN:
                case CYW43_LINK_NOIP:
                case CYW43_LINK_UP:
                    // Connection still in progress or completed
                    g_wifi_state.netif = netif_default;
                    if (wifi_is_connected() && has_valid_address()) {
                        // Successful
                        g_wifi_state.ssid_scan_info[g_wifi_state.selected_ssid_index] = SUCCESS;
                        g_wifi_state.cstate = CONNECTED_IP;
                    } else if (time_reached(g_wifi_state.connect_timeout_time)) {
                        // Connection failed with a timeout
                        give_up_connecting(TIMEOUT);
                    }
                    break;
                default:
                    // Fallback -> connection failure
                    give_up_connecting(FAILED);
                    break;
            }
            break;
        case CONNECTED_IP:
            // In this state we should be connected, but the connection could drop at any time
            if (!wifi_is_connected() || !has_valid_address()) {
                // Connection lost
                give_up_connecting(LOST);
                // It may be some time since the last scan, so scan again
                g_wifi_state.cstate = TRY_TO_CONNECT;
            }
            break;
        case STORAGE_EMPTY_ERROR:
            // This state is reached if the storage file contains no SSIDs.
            // Wait for the file to be updated.
            if (!wifi_settings_has_no_wifi_details()) {
                g_wifi_state.cstate = TRY_TO_CONNECT;
            }
            break;
        case INITIALISATION_ERROR:
        case UNINITIALISED:
        case DISCONNECTED:
            // nothing to do
            break;
        default:
            break;
    }
    // trigger again after the period
    g_wifi_state.periodic_worker.next_time =
        delayed_by_ms(g_wifi_state.periodic_worker.next_time,
                      PERIODIC_TIME_MS);
    async_context_add_at_time_worker(
        g_wifi_state.context,
        &g_wifi_state.periodic_worker);
}

int wifi_settings_init() {
    if (g_wifi_state.cstate != UNINITIALISED) {
        return PICO_ERROR_INVALID_STATE;
    }
    // Put wifi-settings library version into the binary info
    bi_decl_if_func_used(bi_program_feature("pico-wifi-settings v" WIFI_SETTINGS_VERSION_STRING));

    // Start with globals in known state
    memset(&g_wifi_state, 0, sizeof(g_wifi_state));
    g_wifi_state.cstate = UNINITIALISED;
    g_wifi_state.cyw43 = &cyw43_state; // from Pico SDK, lib/cyw43-driver (MAC layer)

    // Which country should be used?
    // You can put "country=<xy>" in the WiFi settings file to set a different value.
    // The code <xy> is a two-byte ISO-3166-1 country code
    // such as AU (Australia), SE (Sweden) or GB (United Kingdom).
    char value[2];
    uint value_size = sizeof(value);
    uint32_t country = PICO_CYW43_ARCH_DEFAULT_COUNTRY_CODE; // worldwide default

    if ((wifi_settings_get_value_for_key("country", value, &value_size))
    && (value_size == 2)) {
        country = CYW43_COUNTRY(value[0], value[1], 0);
    }

    // Set the hostname from wifi-settings "name=<xxx>" or use unique board id
    wifi_settings_set_hostname();

    // Hardware init
    g_wifi_state.hw_error_code = cyw43_arch_init_with_country(country);
    if (g_wifi_state.hw_error_code) {
        g_wifi_state.cstate = INITIALISATION_ERROR;
        return g_wifi_state.hw_error_code;
    }

    // After initialisation, any call to LWIP requires this lock (callback functions
    // are always holding it already, but this function is not a callback)
    cyw43_arch_lwip_begin();

    // Set up to connect to an access point
    cyw43_arch_enable_sta_mode();

    // State initialised
    g_wifi_state.connect_timeout_time = make_timeout_time_ms(CONNECT_TIMEOUT_TIME_MS);
    g_wifi_state.scan_holdoff_time = make_timeout_time_ms(INITIAL_SETUP_TIME_MS);
    g_wifi_state.cstate = DISCONNECTED;

    // Use cyw43 async context
    g_wifi_state.context = cyw43_arch_async_context();

    // Start periodic worker
    g_wifi_state.periodic_worker.next_time = g_wifi_state.scan_holdoff_time;
    g_wifi_state.periodic_worker.do_work = wifi_settings_periodic_callback;
    async_context_add_at_time_worker(
        g_wifi_state.context,
        &g_wifi_state.periodic_worker);

#ifdef ENABLE_REMOTE_UPDATE
    // Start remote access service
    g_wifi_state.hw_error_code = wifi_settings_remote_init();
#endif
    // set lwip hostname (overriding the default set by cyw43_cb_tcpip_init)
    netif_set_hostname(netif_default, wifi_settings_get_hostname());

    // Ready to run LWIP functions
    cyw43_arch_lwip_end();

    return g_wifi_state.hw_error_code;
}

void wifi_settings_deinit() {
    if (g_wifi_state.cstate == UNINITIALISED) {
        return;
    }
    ensure_disconnected();
    if (g_wifi_state.context) {
        // stop periodic task
        async_context_remove_at_time_worker(
            g_wifi_state.context,
            &g_wifi_state.periodic_worker);
    }
    g_wifi_state.context = NULL;
    cyw43_arch_deinit();
    g_wifi_state.cstate = UNINITIALISED;
    g_wifi_state.selected_ssid_index = 0;
}

void wifi_settings_connect() {
    if (g_wifi_state.cstate == DISCONNECTED) {
        // Try to connect when periodic worker is next called
        cyw43_arch_lwip_begin();
        if (g_wifi_state.cstate == DISCONNECTED) {
            g_wifi_state.cstate = TRY_TO_CONNECT;
        }
        cyw43_arch_lwip_end();
    }
}

void wifi_settings_disconnect() {
    // Immediate disconnect
    if ((g_wifi_state.cstate != UNINITIALISED)
    && (g_wifi_state.cstate != INITIALISATION_ERROR)) {
        cyw43_arch_lwip_begin();
        ensure_disconnected();
        g_wifi_state.cstate = DISCONNECTED;
        g_wifi_state.selected_ssid_index = 0;
        cyw43_arch_lwip_end();
    }
}

bool wifi_settings_is_connected() {
    bool rc = false;
    if (g_wifi_state.cstate == CONNECTED_IP) {
        // wifi_is_connected calls LWIP functions, so the lock is needed
        cyw43_arch_lwip_begin();
        rc = wifi_is_connected();
        cyw43_arch_lwip_end();
    }
    return rc;
}
