cmake_minimum_required(VERSION 4.3)
project(jvlex C)

set(CMAKE_C_STANDARD 11)

add_library(jvlex_lib STATIC
        lexer.c
        lexer.h
        tokens.h)
target_include_directories(jvlex_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Standalone lexer executable
add_executable(jvlex main.c)
target_link_libraries(jvlex PRIVATE jvlex_lib)