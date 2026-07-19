cmake_minimum_required(VERSION 3.16)
project(xpresspp_sandbox_api CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Drogon REQUIRED)
find_package(Jsoncpp REQUIRED)
find_package(OpenSSL REQUIRED)

add_executable(xpresspp_sandbox_api main.cpp)

target_include_directories(xpresspp_sandbox_api PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../core/include
)

target_link_libraries(xpresspp_sandbox_api PRIVATE Drogon::Drogon OpenSSL::Crypto)

if(TARGET Jsoncpp_lib)
    target_link_libraries(xpresspp_sandbox_api PRIVATE Jsoncpp_lib)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(xpresspp_sandbox_api PRIVATE JsonCpp::JsonCpp)
else()
    target_include_directories(xpresspp_sandbox_api PRIVATE /usr/include/jsoncpp)
    target_link_libraries(xpresspp_sandbox_api PRIVATE jsoncpp)
endif()
