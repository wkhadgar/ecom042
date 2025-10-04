cmake_minimum_required(VERSION 3.20.0)

set(BOARD_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(BOARD "sua_placa")

find_package(Zephyr REQUIRED)
project(zephyr-template)
add_link_options(--print-memory-usage,-Map=${PROJECT_BINARY_DIR}/${PROJECT_NAME}.map)

include_directories(BEFORE src/)

target_sources(app PRIVATE
        src/main.c
)
