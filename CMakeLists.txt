cmake_minimum_required(VERSION 3.20)

project(kv_store)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(kv_store_lib
    src/kv_store.cpp
)

target_include_directories(kv_store_lib PUBLIC include)

add_executable(kv_store
    src/main.cpp
)

target_link_libraries(kv_store PRIVATE kv_store_lib)

enable_testing()

add_executable(kv_store_tests
    tests/kv_store_test.cpp
)

target_link_libraries(kv_store_tests PRIVATE kv_store_lib)

add_test(NAME kv_store_tests COMMAND kv_store_tests)

add_executable(tcp_server
    src/tcp_server.cpp
)