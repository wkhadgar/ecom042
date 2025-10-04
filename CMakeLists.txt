cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(ZEPHYR_EXTRA_MODULES "${CMAKE_CURRENT_SOURCE_DIR}/camera_service/")

find_package(Zephyr REQUIRED)
project(zephyr-ecom042)
add_link_options(--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)

include_directories(BEFORE app/include)

target_sources(app PRIVATE
        app/src/main.c
        app/src/sensor_thread.c
        app/src/occurrences.c
        app/src/display_thread.c
)
