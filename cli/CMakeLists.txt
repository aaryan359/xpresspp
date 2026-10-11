cmake_minimum_required(VERSION 3.16)
project(xp_cli CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(xp
    src/main.cpp
    src/commands/build.cpp
    src/commands/clean.cpp
    src/commands/create.cpp
    src/commands/dev.cpp
    src/commands/doctor.cpp
    src/commands/generate.cpp
    src/util/console.cpp
    src/util/diagnostics.cpp
    src/util/paths.cpp
    src/util/process.cpp
)

target_include_directories(xp PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
