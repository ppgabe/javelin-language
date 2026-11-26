cmake_minimum_required(VERSION 4.3)
project(jvlnc C)

set(CMAKE_C_STANDARD 11)

add_subdirectory(lexer)
add_subdirectory(syntax_analyzer)

add_executable(jvlnc main.c)
target_link_libraries(jvlnc PRIVATE jvl_parse_lib)
