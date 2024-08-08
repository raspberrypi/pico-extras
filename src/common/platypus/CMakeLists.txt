add_library(platypus INTERFACE)

target_sources(platypus INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/platypus.c
        $<$<BOOL:${PICO_RP2040}>:${CMAKE_CURRENT_LIST_DIR}/decompress_row.S>
        $<$<BOOL:${PICO_RP2350}>:${CMAKE_CURRENT_LIST_DIR}/decompress_row_33.S>
        )

target_include_directories(platypus INTERFACE ${CMAKE_CURRENT_LIST_DIR})

if (PICO_ON_DEVICE)
    target_link_libraries(platypus INTERFACE hardware_interp)
endif()