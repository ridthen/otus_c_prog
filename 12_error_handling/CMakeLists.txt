cmake_minimum_required(VERSION 3.10)
project(LoggerLib C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wextra -Wpedantic -pthread")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -rdynamic")

add_library(logger STATIC logger.c)
target_include_directories(logger PUBLIC include)
target_link_libraries(logger pthread)

add_executable(test_logger sample_app.c)
target_link_libraries(test_logger logger pthread dl)
target_include_directories(test_logger PRIVATE include)