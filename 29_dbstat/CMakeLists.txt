cmake_minimum_required(VERSION 3.10)
project(29_dbstat C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -Wpedantic)

find_library(MARIADB_LIB NAMES mariadb mysqlclient PATHS /opt/homebrew/lib
        /usr/lib)
if(NOT MARIADB_LIB)
    message(FATAL_ERROR "MariaDB/MySQL client library not found")
endif()

find_path(MARIADB_INCLUDE NAMES mysql.h PATHS /opt/homebrew/include/mysql
        /usr/include/mariadb)
if(NOT MARIADB_INCLUDE)
    message(FATAL_ERROR "mysql.h not found")
endif()

add_executable(29_dbstat main.c)
target_include_directories(29_dbstat PRIVATE ${MARIADB_INCLUDE})
target_link_libraries(29_dbstat PRIVATE ${MARIADB_LIB})