cmake_minimum_required(VERSION 4.3)
project(jvlnc C)

set(CMAKE_C_STANDARD 11)

add_executable(jvlex lexer/main.c
        lexer/lexer.c
        lexer/lexer.h
        lexer/tokens.h)
