cmake_minimum_required(VERSION 3.22)
project(socket)

set(CMAKE_CXX_STANDARD 17)

add_executable(socket chatroom_server.cpp)
