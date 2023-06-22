cmake_minimum_required(VERSION 3.24)
project(ujavac)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ujavac
    src/ujavac.h
    src/code.cpp
    src/main.cpp
)
