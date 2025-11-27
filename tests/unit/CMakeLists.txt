cmake_minimum_required(VERSION 3.20.0)
find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})

project(radar_classifier_unit_tests)

target_include_directories(app PRIVATE ../../app/include)

target_sources(app PRIVATE
        src/main.c
        ../../app/src/sensor_thread.c
        ../../app/src/occurrences.c
)
