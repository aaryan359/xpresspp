cmake_minimum_required(VERSION 3.16)
project(xpresspp_app CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── Locate Drogon ─────────────────────────────────────────────────────────────
# First try a system-installed Drogon (fast path).
# If not found, download and build it automatically with FetchContent.
find_package(Drogon QUIET)

if(NOT Drogon_FOUND)
    message(STATUS "[xpress++] Drogon not found on system — fetching from GitHub...")
    message(STATUS "[xpress++] This will take a few minutes on the first build.")
    message(STATUS "[xpress++] Subsequent builds will be instant (cached).")

    include(FetchContent)

    FetchContent_Declare(
        drogon
        GIT_REPOSITORY https://github.com/drogonframework/drogon.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES "lib/trantor"
    )

    # Build Drogon with minimal options — no ORM, no examples, no CTL
    set(BUILD_CTL     OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_ORM     OFF CACHE BOOL "" FORCE)

    FetchContent_MakeAvailable(drogon)
    message(STATUS "[xpress++] Drogon built from source.")
endif()

# ── Locate jsoncpp ────────────────────────────────────────────────────────────
# Drogon bundles jsoncpp as a submodule, so if we fetched Drogon above
# the jsoncpp target already exists. Only do a system find if needed.
if(NOT TARGET jsoncpp_lib AND NOT TARGET JsonCpp::JsonCpp AND NOT TARGET Jsoncpp_lib)
    find_package(jsoncpp QUIET)
    if(NOT jsoncpp_FOUND)
        include(FetchContent)
        FetchContent_Declare(
            jsoncpp
            GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
            GIT_TAG        master
            GIT_SHALLOW    TRUE
        )
        set(JSONCPP_WITH_TESTS    OFF CACHE BOOL "" FORCE)
        set(JSONCPP_WITH_EXAMPLE  OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(jsoncpp)
    endif()
endif()

# ── Build the app ─────────────────────────────────────────────────────────────
add_executable(${PROJECT_NAME} main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}
    ${CMAKE_CURRENT_LIST_DIR}/vendor/xpresspp/include
    ${CMAKE_CURRENT_LIST_DIR}/../../core/include
)

# Link Drogon (covers both system-installed and FetchContent paths)
target_link_libraries(${PROJECT_NAME} PRIVATE Drogon::Drogon)

# Link jsoncpp — handle all known target names
if(TARGET jsoncpp_lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE jsoncpp_lib)
elseif(TARGET JsonCpp::JsonCpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE JsonCpp::JsonCpp)
elseif(TARGET Jsoncpp_lib)
    target_link_libraries(${PROJECT_NAME} PRIVATE Jsoncpp_lib)
else()
    # Last resort: link by name (relies on pkg-config or system path)
    target_include_directories(${PROJECT_NAME} PRIVATE /usr/include/jsoncpp)
    target_link_libraries(${PROJECT_NAME} PRIVATE jsoncpp)
endif()
