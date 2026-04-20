cmake_minimum_required(VERSION 3.10)
project(30_cube C)

set(CMAKE_C_STANDARD 11)
add_compile_options(-Wall -Wextra -Wpedantic)

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)

add_executable(30_cube
        main.c)

target_link_libraries(30_cube ${OPENGL_LIBRARIES} ${GLUT_LIBRARIES})
