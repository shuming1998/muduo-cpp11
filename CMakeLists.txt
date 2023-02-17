cmake_minimum_required(VERSION 3.15)
project(cmuduo)

include_directories(${PROJECT_SOURCE_DIR}/include)

set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

add_subdirectory(src)


