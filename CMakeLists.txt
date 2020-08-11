cmake_minimum_required (VERSION 3.13)

set (CMAKE_CXX_FLAGS "-std=gnu++17 -Wall -O3")
set (CMAKE_CXX_FLAGS_Debug "-DDEBUG -gwarf-2")

add_subdirectory (src)
