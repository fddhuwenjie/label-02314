cmake_minimum_required(VERSION 3.16)
project(ChatServer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Widgets)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/server.cpp
    src/server.h
    src/database.cpp
    src/database.h
    src/loginwindow.cpp
    src/loginwindow.h
    src/mainwindow.cpp
    src/mainwindow.h
    src/logger.cpp
    src/logger.h
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Widgets
)

# 测试选项
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
