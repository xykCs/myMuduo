cmake_minimum_required(VERSION 3.0)
project(mymuduo)

# mymuduo最终编译成so动态库,设置动态库的输出路径，放在根目录的lib文件夹下
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)
# 设置调试信息
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -fPIC")

# 定义参与编译的源文件
aux_source_directory(. SRC_LIST)
# 编译动态库
add_library(mymuduo SHARED ${SRC_LIST})