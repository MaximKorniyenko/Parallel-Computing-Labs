cmake_minimum_required(VERSION 4.1)
project(lab3)

set(CMAKE_CXX_STANDARD 20)

add_executable(lab3 Lab3/lab3.cpp
        Lab3/TaskQueue.h
        Lab3/ThreadPool.h
        Lab3/ThreadPool.cpp
)
