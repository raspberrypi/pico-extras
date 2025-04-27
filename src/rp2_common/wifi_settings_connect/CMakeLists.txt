#
# Copyright (c) 2025 Jack Whitham
#   
# SPDX-License-Identifier: BSD-3-Clause
#   
# wifi_settings_connect
#
# Library to manage WiFi connections. It provides Flash storage
# for WiFi passwords and hotspot names, and a background async_context
# service to automatically connect to them.
#

if (NOT PICO_CYW43_SUPPORTED)
    message("wifi_settings_connect: WiFi hardware is required: run cmake -DPICO_BOARD=pico_w or -DPICO_BOARD=pico2_w")
elseif (NOT TARGET pico_cyw43_arch)
    message("wifi_settings_connect: WiFi driver pico_cyw43_arch is not present")
elseif (NOT TARGET pico_lwip_core)
    message("wifi_settings_connect: IP layer pico_lwip_core is not present")
else()
    message("wifi_settings_connect: library is available.")
    add_library(wifi_settings_connect INTERFACE)

    target_include_directories(wifi_settings_connect INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/include
    )

    target_sources(wifi_settings_connect INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/wifi_settings_connect.c
            ${CMAKE_CURRENT_LIST_DIR}/wifi_settings_flash_storage.c
            ${CMAKE_CURRENT_LIST_DIR}/wifi_settings_flash_range.c
            ${CMAKE_CURRENT_LIST_DIR}/wifi_settings_hostname.c
    )

    target_link_libraries(wifi_settings_connect INTERFACE
            pico_async_context_base
            pico_stdlib
            pico_cyw43_arch
            pico_lwip_core
    )
endif()
