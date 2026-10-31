cmake_minimum_required(VERSION 3.24)

project(mempass VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

option(MEMPASS_BUILD_TESTS "Build unit and integration tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Sql)

add_subdirectory(common)
add_subdirectory(client)
add_subdirectory(server)

if(MEMPASS_BUILD_TESTS)
    include(CTest)
    if(BUILD_TESTING)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        add_subdirectory(tests)
    endif()
endif()
