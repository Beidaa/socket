cmake_minimum_required(VERSION 3.22)
project(socket)

set(CMAKE_CXX_STANDARD 17)

add_executable(socket 9-epoll.cpp 9_5.cpp)
