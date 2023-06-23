cmake_minimum_required(VERSION 3.24)
project(ujavac)

if(MSVC)
    add_compile_options(/MP /GR- /EHsc)
endif()

add_subdirectory(third_party/oneTBB)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(WIN32)
    add_definitions(-DNOMINMAX -D_UNICODE -DUNICODE)
endif()

add_executable(ujavac
    src/ujavac.h
    src/compiler.cpp
    src/main.cpp
)

target_link_libraries(ujavac PRIVATE tbb)
