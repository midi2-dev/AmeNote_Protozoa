cmake_minimum_required(VERSION 3.12)

project(FreeRTOS_Tasks_Tests CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

find_package(GTest "1.11.0" REQUIRED)

add_executable(unittests UMPRingBuffer.tests.cpp)
target_link_libraries(unittests PRIVATE GTest::GTest GTest::gmock_main)

