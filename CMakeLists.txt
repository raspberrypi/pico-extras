cmake_minimum_required(VERSION 3.13)

if (NOT TARGET _pico_extras_inclusion_marker)
    add_library(_pico_extras_inclusion_marker INTERFACE)

    # Pull in PICO SDK (must be before project)
    include(pico_sdk_import.cmake)

    project(pico_extras C CXX)

    set(CMAKE_C_STANDARD 11)
    set(CMAKE_CXX_STANDARD 17)

    # Initialize the SDK
    pico_sdk_init()

    pico_is_top_level_project(PICO_EXTRAS_TOP_LEVEL_PROJECT)

    add_library(pico_extras_included INTERFACE)
    target_compile_definitions(pico_extras_included INTERFACE
            -DPICO_EXTRAS=1
    )

    pico_add_platform_library(pico_extras_included)

    if (NOT PICO_EXTRAS_PATH)
        set(PICO_EXTRAS_PATH ${CMAKE_CURRENT_LIST_DIR})
    endif()
    set(PICO_EXTRAS_PATH "${PICO_EXTRAS_PATH}" CACHE PATH "Path to Pico Extras")

    add_subdirectory(src)

    if (PICO_EXTRAS_TESTS_ENABLED OR PICO_EXTRAS_TOP_LEVEL_PROJECT)
        add_subdirectory(test)
    endif ()

endif()