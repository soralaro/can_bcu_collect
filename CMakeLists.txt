cmake_minimum_required(VERSION 3.10)
set(TARGET_NAME "Collect_bcu_data")
project(${TARGET_NAME})
set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)
set(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall -std=c++17 -fpermissive -fno-strict-aliasing -fopenmp")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -Wall  -fopenmp")

file(GLOB SOURCES "src/*.cpp")

include_directories(include)

add_executable(${TARGET_NAME} ${SOURCES})