cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(EXTRA_DTC_OVERLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/boards/stm32_min_dev.overlay)

find_package(Zephyr REQUIRED)
project(zephyr-ecom042)
add_link_options(--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)

include_directories(BEFORE src/)

target_sources(app PRIVATE
        src/main.c
)
