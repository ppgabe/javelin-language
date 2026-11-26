cmake_minimum_required(VERSION 4.3)
project(jvl_parse C)

set(CMAKE_C_STANDARD 11)

add_executable(jvl_parse main.c
        lexer.h
        tokens.h)
