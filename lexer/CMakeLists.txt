cmake_minimum_required(VERSION 4.3)
project(jvlex C)

set(CMAKE_C_STANDARD 11)

add_executable(jvlex main.c
        lexer.c
        lexer.h
        tokens.h)
