cmake_minimum_required(VERSION 3.5.0)
project(LANGAME VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/libs)

#find_package(GTest REQUIRED)
#include_directories(${GTEST_INCLUDE_DIRS})

add_executable(server Server/Server.cpp)
add_executable(client Client/Client.cpp)

#add_executable(server_tests 
 #   tests/server_tests.cpp

#)

#target_link_libraries(server_tests
#   ${GTEST_LIBRARIES}
 #   pthread
#)

include(CTest)
enable_testing()

#add_test(NAME ServerTests COMMAND server_tests)

#set_target_properties(server_tests PROPERTIES
#    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
#)


