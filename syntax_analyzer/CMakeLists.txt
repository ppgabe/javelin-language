cmake_minimum_required(VERSION 4.3)
project(jvl_parse C)

set(CMAKE_C_STANDARD 11)

add_library(jvl_parse_lib STATIC
        parser.c
        parser.h)

target_include_directories(jvl_parse_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(jvl_parse_lib PUBLIC jvlex_lib)