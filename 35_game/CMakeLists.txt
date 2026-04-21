cmake_minimum_required(VERSION 3.10)
project(35_game C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_compile_options(-Wall -Wextra -Wpedantic)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ALLEGRO REQUIRED
        allegro-5
        allegro_primitives-5
        allegro_main-5
        allegro_font-5
        allegro_ttf-5
)

add_executable(35_game main.c)

target_include_directories(35_game PRIVATE ${ALLEGRO_INCLUDE_DIRS})
target_link_libraries(35_game ${ALLEGRO_LDFLAGS})
target_compile_options(35_game PRIVATE ${ALLEGRO_CFLAGS_OTHER})