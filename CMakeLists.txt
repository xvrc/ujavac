cmake_minimum_required(VERSION 3.24)
project(ujavac)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

if(MSVC)
    add_compile_options(/MP /GR- /EHsc /utf-8)
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
