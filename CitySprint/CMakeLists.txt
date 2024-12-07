cmake_minimum_required(VERSION 3.10)
project(WebSocketGame)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)

# Create our executable
add_executable(CitySprint "game_server.cpp" "Logger.cpp" "Utilities.cpp" "ThreadingUtilities.cpp" "QuadTreeOO.h" "QuadTreeOOImplementation.h")

if (WIN32)
    set(CMAKE_SYSTEM_NAME Windows)
    target_link_libraries(CitySprint ${CMAKE_SOURCE_DIR}/lib/libssl.lib ${CMAKE_SOURCE_DIR}/lib/libcrypto.lib ws2_32)
else()
    find_package(OpenSSL REQUIRED)
    target_link_libraries(CitySprint OpenSSL::SSL OpenSSL::Crypto pthread)
endif()