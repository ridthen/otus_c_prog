cmake_minimum_required(VERSION 3.31)
project(21_telnet C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(21_telnet main.c)
