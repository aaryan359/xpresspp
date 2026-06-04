cmake_minimum_required(VERSION 3.16)
project(xpresspp_core CXX)

add_library(xpresspp INTERFACE)
add_library(xpresspp::xpresspp ALIAS xpresspp)

target_compile_features(xpresspp INTERFACE cxx_std_20)

target_include_directories(xpresspp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

find_package(Drogon REQUIRED)
find_package(Jsoncpp REQUIRED)

target_link_libraries(xpresspp INTERFACE Drogon::Drogon)

if(TARGET Jsoncpp_lib)
    target_link_libraries(xpresspp INTERFACE Jsoncpp_lib)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(xpresspp INTERFACE JsonCpp::JsonCpp)
else()
    target_include_directories(xpresspp INTERFACE /usr/include/jsoncpp)
    target_link_libraries(xpresspp INTERFACE jsoncpp)
endif()
