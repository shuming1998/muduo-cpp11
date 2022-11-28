cmake_minimum_required(VERSION 3.15)
project(myMuduo)

# 设置动态库路径：根目录下的lib文件夹
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
#设置调试信息 以及设置c++11
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fPIC")

# 定义参与编译的源代码文件
aux_source_directory(. SRC)
#编译生成动态库mymuduo
add_library(myMuduo SHARED ${SRC})