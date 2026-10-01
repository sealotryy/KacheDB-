cmake_minimum_required(VERSION 3.20)

project(kv_store)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(kv_store
    src/main.cpp
    src/kv_store.cpp
)

target_include_directories(kv_store PRIVATE include)