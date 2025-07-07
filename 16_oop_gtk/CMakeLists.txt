cmake_minimum_required(VERSION 3.28)
project(treeview C)

set(CMAKE_C_STANDARD 11)

#find_package(PkgConfig REQUIRED)
FIND_PACKAGE(PkgConfig REQUIRED)
#pkg_check_modules (GTK4 REQUIRED gtk4)
pkg_check_modules (GTK3 REQUIRED gtk+-3.0)

# Setup CMake to use GTK+, tell the compiler where to look for headers
# and to the linker where to look for libraries
#INCLUDE_DIRECTORIES(${GTK4_INCLUDE_DIRS})
#LINK_DIRECTORIES(${GTK4_LIBRARY_DIRS})
#
## Add other flags to the compiler
#ADD_DEFINITIONS(${GTK4_CFLAGS_OTHER})

INCLUDE_DIRECTORIES(${GTK3_INCLUDE_DIRS})
LINK_DIRECTORIES(${GTK3_LIBRARY_DIRS})

# Add other flags to the compiler
ADD_DEFINITIONS(${GTK3_CFLAGS_OTHER})

add_executable(treeview main.c)
#target_link_libraries(treeview ${GTK4_LIBRARIES})
target_link_libraries(treeview ${GTK3_LIBRARIES})