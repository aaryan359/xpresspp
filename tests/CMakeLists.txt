cmake_minimum_required(VERSION 3.16)
project(xpresspp_tests CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()

if(NOT TARGET xpresspp::xpresspp)
    find_package(Drogon REQUIRED)
    find_package(OpenSSL REQUIRED)
endif()

add_executable(xpresspp_core_tests core_tests.cpp)
if(TARGET xpresspp::xpresspp)
    target_link_libraries(xpresspp_core_tests PRIVATE xpresspp::xpresspp)
else()
    target_include_directories(xpresspp_core_tests PRIVATE ../core/include)
    target_link_libraries(xpresspp_core_tests PRIVATE Drogon::Drogon OpenSSL::Crypto)
endif()

add_test(NAME xpresspp_core_tests COMMAND xpresspp_core_tests)
