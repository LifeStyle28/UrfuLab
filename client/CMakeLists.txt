#include_directories(${CMAKE_SOURCE_DIR}/logger)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt5 REQUIRED COMPONENTS Core Network)

add_executable(client
        tcpclient.h
        tcpclient.cpp
        main.cpp
        )
target_link_libraries(client PRIVATE Qt5::Network Qt5::Core CONAN_PKG::boost)