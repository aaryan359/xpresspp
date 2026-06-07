cmake_minimum_required(VERSION 3.16)
project(xpresspp_benchmark CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Drogon REQUIRED)
find_package(jsoncpp REQUIRED)

add_executable(xpresspp_benchmark main.cpp)

target_include_directories(xpresspp_benchmark PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../core/include
)

target_link_libraries(xpresspp_benchmark PRIVATE 
    Drogon::Drogon 
    jsoncpp
)

target_compile_options(xpresspp_benchmark PRIVATE -O3 -march=native -flto)
target_link_options(xpresspp_benchmark PRIVATE -O3 -flto)
