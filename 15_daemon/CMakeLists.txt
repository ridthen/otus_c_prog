cmake_minimum_required(VERSION 3.10)
project(filesizer C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

find_library(LIBYAML_LIB yaml)
find_path(LIBYAML_INCLUDE_DIR yaml.h)

add_executable(filesizer
    main.c
    config.c
    server.c
)

target_include_directories(filesizer PRIVATE ${LIBYAML_INCLUDE_DIR})
target_link_libraries(filesizer ${LIBYAML_LIB})

