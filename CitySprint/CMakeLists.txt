﻿cmake_minimum_required(VERSION 3.10)
project(WebSocketGame)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

#Create our executable
add_executable(CitySprint "game_server.cpp")

# Link libraries
target_link_libraries(CitySprint ${CMAKE_SOURCE_DIR}/lib/libssl.lib ${CMAKE_SOURCE_DIR}/lib/libcrypto.lib)
target_link_libraries(CitySprint ws2_32)