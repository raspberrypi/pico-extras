/**
 * Copyright (c) 2025 Jack Whitham
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * This pico-wifi-settings module reads WiFi settings
 * information in Flash.
 *
 */

#include "wifi_settings/wifi_settings_flash_storage.h"
#include "wifi_settings/wifi_settings_flash_range.h"

#include <string.h>


bool wifi_settings_get_value_for_key(
            const char* key, char* value, uint* value_size) {

    wifi_settings_flash_range_t fr;
    wifi_settings_logical_range_t lr;

    wifi_settings_range_get_wifi_settings_file(&fr);
    wifi_settings_range_translate_to_logical(&fr, &lr);

    const char* file = (const char*) lr.start_address;
    const uint file_size = lr.size;

    enum parse_state_t {
        NEW_LINE,
        KEY,
        SEPARATOR,
        VALUE,
        WAIT_FOR_NEW_LINE,
    } parse_state = NEW_LINE;
    uint value_index = 0;
    uint key_index = 0;

    if (key[0] == '\0') {
        // Invalid key - must contain at least 1 character
        return false;
    }

    for (uint file_index = 0;
            (file_index < file_size)
            && (file[file_index] != '\0')
            && (file[file_index] != '\x1a') // CPM EOF character
            && (file[file_index] != '\xff'); // Flash padding character
            file_index++) {

        if ((file[file_index] == '\n') || (file[file_index] == '\r')) {
            // End of line reached (Unix or DOS line endings)
            if (parse_state == VALUE) {
                // This is the end of the value
                *value_size = value_index;
                return true;
            } else {
                // Reset the parsing state
                parse_state = NEW_LINE;
                continue;
            }
        }

        switch (parse_state) {
            case NEW_LINE:
                // At the beginning of a new line - ignore whitespace before the key
                key_index = 0;
                if (key[key_index] == file[file_index]) {
                    // Matched the first character in the key
                    key_index++;
                    if (key[key_index] == '\0') {
                        // There is only one character in the key
                        parse_state = SEPARATOR;
                    } else {
                        // Match the other characters in the key
                        parse_state = KEY;
                    }
                } else {
                    // Non-matching character: a different key,
                    // a comment - wait for the next newline
                    parse_state = WAIT_FOR_NEW_LINE;
                }
                break;
            case KEY:
                if (key[key_index] == file[file_index]) {
                    // Still matching the key
                    key_index++;
                    if (key[key_index] == '\0') {
                        // There are no more characters in the key
                        parse_state = SEPARATOR;
                    }
                } else {
                    // Non-matching character in the key
                    parse_state = WAIT_FOR_NEW_LINE;
                }
                break;
            case SEPARATOR:
                if (file[file_index] == '=') {
                    // Key is recognised - copy the value
                    parse_state = VALUE;
                } else {
                    // Key is not immediately followed by '=': not valid
                    parse_state = WAIT_FOR_NEW_LINE;
                }
                break;
            case VALUE:
                if (value_index >= *value_size) {
                    // Unable to copy more value characters - value is complete
                    return true;
                } else {
                    value[value_index] = file[file_index];
                    value_index++;
                }
                break;
            case WAIT_FOR_NEW_LINE:
                // Do nothing in this state, as the state will be reset when
                // the next newline is seen
                break;
        }
    }
    if (parse_state == VALUE) {
        // Reached end of file while parsing the value - value is complete
        *value_size = value_index;
        return true;
    }
    // Key was not found
    return false;
}
