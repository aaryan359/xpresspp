cmake_minimum_required(VERSION 3.16)
project(xp_cli CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add CLI executable with all modular source files
add_executable(xp
    src/main.cpp
    src/common.cpp
    src/diagnostics.cpp
    src/commands.cpp
)

# Add local include/ directory to compilation path
target_include_directories(xp PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
