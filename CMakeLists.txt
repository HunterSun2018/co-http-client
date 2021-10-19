cmake_minimum_required(VERSION 3.0)

project(coroutine-demo VERSION 1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED true)
set(CMAKE_CXX_COMPILER "g++-10")
set(CMAKE_CXX_FLAGS "-fcoroutines")

aux_source_directory(src SRC)

add_executable(demo ${SRC})

target_link_libraries(demo pthread ssl crypto)

