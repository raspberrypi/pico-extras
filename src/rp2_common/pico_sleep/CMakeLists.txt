pico_simple_hardware_target(sleep)
pico_mirrored_target_link_libraries(hardware_sleep INTERFACE
        hardware_clocks
        hardware_rosc
        hardware_irq
        pico_aon_timer
        )
target_include_directories(hardware_sleep INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/include
        )
