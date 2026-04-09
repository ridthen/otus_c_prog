cmake_minimum_required(VERSION 3.31)
project(16_crc32 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(16_crc32 main.c)
