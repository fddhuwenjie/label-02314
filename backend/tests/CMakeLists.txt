cmake_minimum_required(VERSION 3.16)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql Test)

enable_testing()

# Database 测试
add_executable(test_database
    test_database.cpp
    ../src/database.cpp
    ../src/database.h
    ../src/logger.cpp
    ../src/logger.h
)
target_link_libraries(test_database PRIVATE
    Qt6::Core
    Qt6::Sql
    Qt6::Test
)
add_test(NAME DatabaseTests COMMAND test_database)

# Server 测试
add_executable(test_server
    test_server.cpp
    ../src/server.cpp
    ../src/server.h
    ../src/database.cpp
    ../src/database.h
    ../src/logger.cpp
    ../src/logger.h
)
target_link_libraries(test_server PRIVATE
    Qt6::Core
    Qt6::Network
    Qt6::Sql
    Qt6::Test
)
add_test(NAME ServerTests COMMAND test_server)
