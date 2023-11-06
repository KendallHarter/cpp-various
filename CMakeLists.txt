cmake_minimum_required(VERSION 3.28)

project(cpp_various)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_SCAN_FOR_MODULES TRUE)

if (MSVC)
   add_compile_options(/W4)
else()
   add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(any_no_rtti src/any_no_rtti.cpp)
add_executable(coroutines0 src/coroutines0.cpp)
add_executable(coroutines1_client src/coroutines1/client.cpp)
add_executable(coroutines1_server src/coroutines1/server.cpp)
add_executable(huffman_encoding src/huffman_encoding.cpp)

add_executable(modules_test src/modules/main.cpp)
target_sources(modules_test PRIVATE
   FILE_SET all_modules TYPE CXX_MODULES
   BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
   FILES src/modules/module_test.cpp src/modules/module_test_a.cpp
)
