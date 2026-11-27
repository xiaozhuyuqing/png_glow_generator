cmake_minimum_required(VERSION 3.16)
project(png_glow_generator LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(PNG REQUIRED)

add_executable(png_glow_generator src/main.cpp src/glow_converter.cpp)
target_link_libraries(png_glow_generator PRIVATE PNG::PNG)
