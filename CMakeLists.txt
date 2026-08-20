cmake_minimum_required(VERSION 3.16)
project(library_management_system VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(library_management_system src/main.cpp)

if(MSVC)
    target_compile_options(library_management_system PRIVATE /W4 /permissive-)
else()
    target_compile_options(library_management_system PRIVATE -Wall -Wextra -Wpedantic)
endif()
