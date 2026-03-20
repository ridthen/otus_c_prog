cmake_minimum_required(VERSION 3.10)
project(PointWaveModel C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wpedantic -g")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -Wpedantic -O2")
endif()

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lm")

find_package(PkgConfig REQUIRED)
pkg_check_modules(NETCDF REQUIRED IMPORTED_TARGET netcdf)

set(SOURCES
        spectrum.c
        source_terms.c
        integrator.c
        point_wave_model.c
        netcdf_output.c
)

add_executable(point_wave_model ${SOURCES})

target_link_libraries(point_wave_model PkgConfig::NETCDF)

target_include_directories(point_wave_model PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
