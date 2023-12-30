cmake_minimum_required(VERSION 3.11)

project(monitor CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wformat -Wformat-security -Werror")

include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
conan_basic_setup(TARGETS)

find_package(Boost 1.78.0 REQUIRED)
if(Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
endif()

add_subdirectory(logger)
add_subdirectory(client)
add_subdirectory(monitor)
add_subdirectory(server)

include_directories(
    client
    monitor
    server
    logger
)
