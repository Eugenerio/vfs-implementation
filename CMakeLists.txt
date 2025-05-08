cmake_minimum_required(VERSION 3.10)
project(FileSystem)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(vfs
    main.cpp
    filesystem.cpp
)

target_include_directories(vfs PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}) 