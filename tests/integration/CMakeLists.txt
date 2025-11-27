cmake_minimum_required(VERSION 3.20.0)
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(ZEPHYR_EXTRA_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/../../camera_service")

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(radar_classifier_integration_tests)

target_include_directories(app PRIVATE ../../app/include)

target_sources(app PRIVATE
        src/main.c
        ../../app/src/sensor_thread.c
        ../../app/src/occurrences.c
        ../../app/src/display_thread.c
)
