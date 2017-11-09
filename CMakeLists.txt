cmake_minimum_required(VERSION 3.5)
project(FastCircularQueue)

set(CMAKE_CXX_STANDARD 11)

set(SOURCE_FILES FastCircularQueue.h main.cpp)

add_executable(FastCircularQueue ${SOURCE_FILES})