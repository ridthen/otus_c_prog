cmake_minimum_required(VERSION 3.25)
project(08_search_sort_homework C)
add_compile_options(-Wall -Wextra -Wpedantic -std=c11 -ggdb3 -Og)

set(CMAKE_C_STANDARD 11)

add_executable(wordFreq hashmap_open.c)

