cmake_minimum_required(VERSION 3.15)

project(DatabaseC)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(DatabaseC
    DatabaseC.cpp
    DatabaseC.h
    src/Server.cpp
    src/Server.h
    src/BinaryFile.h
    src/BinaryFile.cpp
    src/DataModels.h
    src/Table.h
    src/Table.cpp
    src/Database.cpp
    src/Database.h
    src/DataModels.cpp
)

target_include_directories(DatabaseC PRIVATE
    ${CMAKE_SOURCE_DIR}/external
    ${CMAKE_SOURCE_DIR}/external/boost_1_92_0
)

target_compile_definitions(DatabaseC PRIVATE
    CROW_USE_BOOST
)