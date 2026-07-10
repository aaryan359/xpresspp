cmake_minimum_required(VERSION 3.16)
project(xpresspp VERSION 0.1.0 LANGUAGES CXX)

include(CTest)
add_subdirectory(core)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
