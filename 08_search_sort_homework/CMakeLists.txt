cmake_minimum_required(VERSION 3.25)
project(08_search_sort_homework C)

set(CMAKE_C_STANDARD 11)

add_executable(wordFreq hashmap_open.c)
add_compile_options(-Wall -Wextra -Wpedantic -std=c11)
