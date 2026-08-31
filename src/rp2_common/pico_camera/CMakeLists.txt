if (NOT TARGET pico_camera)
    add_library(pico_camera INTERFACE)

    target_sources(pico_camera INTERFACE
            ${CMAKE_CURRENT_LIST_DIR}/pico_camera.cpp
            ${CMAKE_CURRENT_LIST_DIR}/driver/pio_capture.cpp
            ${CMAKE_CURRENT_LIST_DIR}/driver/sccb.cpp
            ${CMAKE_CURRENT_LIST_DIR}/driver/xclk.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sensors/gc0308.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sensors/gc032a.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sensors/gc2145.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sensors/ov2640.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sensors/ov3660.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sensors/ov7670.cpp
            ${CMAKE_CURRENT_LIST_DIR}/sensors/ov7725.cpp
    )

    target_include_directories(pico_camera INTERFACE
            ${CMAKE_CURRENT_LIST_DIR})

    target_link_libraries(pico_camera INTERFACE
            pico_stdlib
            hardware_pio
            hardware_dma
            hardware_i2c
            hardware_pwm
            hardware_gpio
            hardware_clocks)
endif()
