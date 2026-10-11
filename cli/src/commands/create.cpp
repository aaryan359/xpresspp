#include "commands/commands.h"
#include "util/util.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <map>

namespace xp::cli {

// Replaces every @KEY@ in `text` with the matching value.
static std::string fill(std::string text, const std::map<std::string, std::string>& values) {
    for (const auto& [key, value] : values) {
        const std::string marker = "@" + key + "@";
        for (auto at = text.find(marker); at != std::string::npos; at = text.find(marker, at + value.size())) {
            text.replace(at, marker.size(), value);
        }
    }
    return text;
}

static void writeFile(const fs::path& path, const std::string& content) {
    fs::create_directories(path.parent_path());
    std::ofstream(path) << content;
}

static const char* cmakeTemplate = R"(cmake_minimum_required(VERSION 3.16)
project(@NAME@ CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Where the Xpress++ headers live on this machine.
set(XPRESSPP_HOME "@HOME@")

# Use the system Drogon if it is installed, otherwise download it (first build only).
find_package(Drogon QUIET)
if(NOT Drogon_FOUND)
    include(FetchContent)
    FetchContent_Declare(drogon
        GIT_REPOSITORY https://github.com/drogonframework/drogon.git
        GIT_TAG        master
        GIT_SHALLOW    TRUE
        GIT_SUBMODULES "lib/trantor")
    set(BUILD_CTL OFF CACHE BOOL "" FORCE)
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_ORM OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(drogon)
endif()

add_executable(@NAME@ main.cpp)
target_include_directories(@NAME@ PRIVATE ${XPRESSPP_HOME}/core/include)
target_link_libraries(@NAME@ PRIVATE Drogon::Drogon)

# jsoncpp has a different target name depending on how it was installed.
foreach(target jsoncpp_lib JsonCpp::JsonCpp Jsoncpp_lib)
    if(TARGET ${target})
        target_link_libraries(@NAME@ PRIVATE ${target})
        break()
    endif()
endforeach()
)";

static bool isValidName(const std::string& name) {
    if (name.empty()) return false;
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') return false;
    }
    return true;
}

int createApp(const std::string& name) {
    if (!isValidName(name)) {
        error("Invalid project name: \"" + name + "\"",
              "Use only letters, numbers, - and _.", "xp create my-app");
        return 1;
    }

    const fs::path target = fs::current_path() / name;
    if (fs::exists(target)) {
        error("A folder named \"" + name + "\" already exists.", target.string(),
              "Pick another name, or delete the folder.");
        return 1;
    }

    const fs::path root    = frameworkRoot();
    const fs::path example = root / "examples" / "simple";
    if (!fs::exists(example)) {
        error("Could not find the Xpress++ examples.", example.string(),
              "export XPRESSPP_HOME=/path/to/xpress++");
        return 1;
    }

    info("Creating " + name + "...");
    fs::create_directories(target);
    fs::copy_file(example / "main.cpp",     target / "main.cpp");
    fs::copy_file(example / ".env.example", target / ".env.example");
    writeFile(target / "CMakeLists.txt", fill(cmakeTemplate, {{"NAME", name}, {"HOME", root.string()}}));
    writeFile(target / ".gitignore", "build/\n.env\n*.log\n");

    divider();
    success("Created " + name);
    std::cout << colour::cyan()
              << "\n    cd " << name
              << "\n    cp .env.example .env"
              << "\n    xp dev\n\n" << colour::reset();
    return 0;
}

} // namespace xp::cli
