cmake_minimum_required(VERSION 3.16)
project(xpresspp_app CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Drogon REQUIRED)
find_package(Jsoncpp REQUIRED)

add_executable(xpresspp_app main.cpp)

target_include_directories(xpresspp_app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../core/include
)

target_link_libraries(xpresspp_app PRIVATE Drogon::Drogon)

if(TARGET Jsoncpp_lib)
    target_link_libraries(xpresspp_app PRIVATE Jsoncpp_lib)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(xpresspp_app PRIVATE JsonCpp::JsonCpp)
else()
    target_include_directories(xpresspp_app PRIVATE /usr/include/jsoncpp)
    target_link_libraries(xpresspp_app PRIVATE jsoncpp)
endif()
