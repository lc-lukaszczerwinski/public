cmake_minimum_required(VERSION 3.25)

project(HFTDojo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(matching_engine_v3 matching_engine_v3.cpp)
