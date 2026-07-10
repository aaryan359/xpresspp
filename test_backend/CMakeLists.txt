cmake_minimum_required(VERSION 3.16)
project(test_backend CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Set XPRESSPP_HOME path to find core headers
set(XPRESSPP_HOME "/home/aaryan/Desktop/xpress++")

# Try system Drogon first; fall back to downloading from GitHub.
find_package(Drogon QUIET)
if(NOT Drogon_FOUND)
    message(STATUS "[test_backend] Drogon not found — fetching from GitHub (first build only)...")
    include(FetchContent)
    FetchContent_Declare(drogon
        GIT_REPOSITORY https://github.com/drogonframework/drogon.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES "lib/trantor")
    set(BUILD_CTL      OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_ORM      OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(drogon)
endif()

add_executable(test_backend main.cpp)
target_include_directories(test_backend PRIVATE
    ${XPRESSPP_HOME}/core/include
    ${CMAKE_CURRENT_LIST_DIR}/build/generated)
target_link_libraries(test_backend PRIVATE Drogon::Drogon)

if(TARGET jsoncpp_lib)
    target_link_libraries(test_backend PRIVATE jsoncpp_lib)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(test_backend PRIVATE JsonCpp::JsonCpp)
elseif(TARGET Jsoncpp_lib)
    target_link_libraries(test_backend PRIVATE Jsoncpp_lib)
else()
    target_include_directories(test_backend PRIVATE /usr/include/jsoncpp)
    target_link_libraries(test_backend PRIVATE jsoncpp)
endif()
