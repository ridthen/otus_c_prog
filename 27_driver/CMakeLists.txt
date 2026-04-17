cmake_minimum_required(VERSION 3.31)
project(27_test_ringbuf C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

add_executable(27_test_ringbuf test_ringbuf.c)
