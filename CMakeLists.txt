cmake_minimum_required(VERSION 3.5.0)
project(LANGAME VERSION 0.1.0 LANGUAGES C CXX)
include_directories(${CMAKE_SOURCE_DIR}/libs)

add_executable(server Server/Server.cpp)
add_executable(client Client/Client.cpp)


include(CTest)
enable_testing()

