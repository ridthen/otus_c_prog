cmake_minimum_required(VERSION 3.31)
project(23_http_server C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(23_http_server main.c)
