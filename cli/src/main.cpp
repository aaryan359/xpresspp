#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

namespace {

void usage() {
    std::cout
        << "Xpress++ CLI\n\n"
        << "Usage:\n"
        << "  xp create <app-name>\n"
        << "  xp build [--release]\n"
        << "  xp run [--release]\n"
        << "  xp watch\n"
        << "  xp clean\n"
        << "  xp doctor\n";
}

int runCommand(const std::string& command) {
    std::cout << "$ " << command << std::endl;
    return std::system(command.c_str());
}

fs::path repoRoot() {
    if (const char* env = std::getenv("XPRESSPP_HOME")) {
        return fs::path(env);
    }

    if (const char* env = std::getenv("XPRESSPP_CLI_PATH")) {
        auto cli_path = fs::path(env);
        if (fs::exists(cli_path)) {
            auto current = fs::weakly_canonical(cli_path).parent_path();
            while (!current.empty()) {
                if (fs::exists(current / "core" / "include" / "xpresspp" / "xpresspp.h") &&
                    fs::exists(current / "templates" / "default_app")) {
                    return current;
                }
                if (current == current.root_path()) {
                    break;
                }
                current = current.parent_path();
            }
        }
    }

    auto current = fs::current_path();
    while (!current.empty()) {
        if (fs::exists(current / "core" / "include" / "xpresspp" / "xpresspp.h") &&
            fs::exists(current / "templates" / "default_app")) {
            return current;
        }
        if (current == current.root_path()) {
            break;
        }
        current = current.parent_path();
    }

    return fs::current_path();
}

bool shouldSkipTemplatePath(const fs::path& relative) {
    for (const auto& part : relative) {
        const auto name = part.string();
        if (name == "build" || name == ".git" || name == ".vscode" || name == "CMakeFiles") {
            return true;
        }
    }

    const auto filename = relative.filename().string();
    if (filename == "CMakeCache.txt" ||
        filename == "cmake_install.cmake" ||
        filename == "compile_commands.json" ||
        filename == "Makefile") {
        return true;
    }

    const auto ext = relative.extension().string();
    return ext == ".o" || ext == ".obj" || ext == ".a" || ext == ".so" ||
           ext == ".dylib" || ext == ".dll" || ext == ".exe";
}

void copyDirectory(const fs::path& from, const fs::path& to) {
    fs::create_directories(to);
    for (const auto& entry : fs::recursive_directory_iterator(from)) {
        const auto relative = fs::relative(entry.path(), from);
        if (shouldSkipTemplatePath(relative)) {
            continue;
        }

        const auto target = to / relative;

        if (entry.is_directory()) {
            fs::create_directories(target);
        } else if (entry.is_regular_file()) {
            fs::create_directories(target.parent_path());
            fs::copy_file(entry.path(), target, fs::copy_options::overwrite_existing);
        }
    }
}

int createApp(const std::string& name) {
    const fs::path target = fs::current_path() / name;
    if (fs::exists(target)) {
        std::cerr << "Cannot create app: " << target << " already exists.\n";
        return 1;
    }

    const fs::path root = repoRoot();
    const fs::path template_dir = root / "templates" / "default_app";
    const fs::path core_include = root / "core" / "include";

    if (!fs::exists(template_dir) || !fs::exists(core_include)) {
        std::cerr << "Cannot find Xpress++ templates/core. Set XPRESSPP_HOME to the repo path.\n";
        return 1;
    }

    copyDirectory(template_dir, target);
    copyDirectory(core_include, target / "vendor" / "xpresspp" / "include");

    std::ofstream cmake(target / "CMakeLists.txt");
    cmake
        << "cmake_minimum_required(VERSION 3.16)\n"
        << "project(" << name << " CXX)\n\n"
        << "set(CMAKE_CXX_STANDARD 20)\n"
        << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        << "set(CMAKE_CXX_EXTENSIONS OFF)\n\n"
        << "find_package(Drogon REQUIRED)\n"
        << "find_package(Jsoncpp REQUIRED)\n\n"
        << "add_executable(" << name << " main.cpp)\n"
        << "target_include_directories(" << name << " PRIVATE ${CMAKE_CURRENT_LIST_DIR}/vendor/xpresspp/include)\n"
        << "target_link_libraries(" << name << " PRIVATE Drogon::Drogon)\n\n"
        << "if(TARGET Jsoncpp_lib)\n"
        << "    target_link_libraries(" << name << " PRIVATE Jsoncpp_lib)\n"
        << "elseif(TARGET JsonCpp::JsonCpp)\n"
        << "    target_link_libraries(" << name << " PRIVATE JsonCpp::JsonCpp)\n"
        << "else()\n"
        << "    target_include_directories(" << name << " PRIVATE /usr/include/jsoncpp)\n"
        << "    target_link_libraries(" << name << " PRIVATE jsoncpp)\n"
        << "endif()\n";

    std::ofstream gitignore(target / ".gitignore");
    gitignore << "build/\n.vscode/\nuploads/tmp/\n";

    std::cout << "Created Xpress++ app at " << target << "\n";
    std::cout << "Next: cd " << name << " && xp run\n";
    return 0;
}

std::string buildType(bool release) {
    return release ? "Release" : "Debug";
}

int build(bool release) {
    const auto type = buildType(release);
    const fs::path cache = fs::current_path() / "build" / "CMakeCache.txt";
    if (fs::exists(cache)) {
        std::ifstream input(cache);
        std::string line;
        while (std::getline(input, line)) {
            if (line.rfind("CMAKE_HOME_DIRECTORY:INTERNAL=", 0) == 0) {
                const auto cached_source = fs::path(line.substr(30));
                if (fs::weakly_canonical(cached_source) != fs::weakly_canonical(fs::current_path())) {
                    std::cout << "Removing stale CMake build cache from another app.\n";
                    fs::remove_all("build");
                }
                break;
            }
        }
    }

    if (runCommand("cmake -S . -B build -DCMAKE_BUILD_TYPE=" + type) != 0) {
        return 1;
    }
    return runCommand("cmake --build build");
}

std::string projectBinary() {
    fs::path best;
    for (const auto& entry : fs::directory_iterator("build")) {
        if (entry.is_regular_file()) {
            const auto permissions = entry.status().permissions();
            const bool executable =
                (permissions & fs::perms::owner_exec) != fs::perms::none ||
                (permissions & fs::perms::group_exec) != fs::perms::none ||
                (permissions & fs::perms::others_exec) != fs::perms::none;
            if (executable && entry.path().filename() != "cmake_install.cmake") {
                best = entry.path();
            }
        }
    }
    return best.string();
}

int run(bool release) {
    if (build(release) != 0) {
        return 1;
    }

    const auto binary = projectBinary();
    if (binary.empty()) {
        std::cerr << "Build succeeded, but no executable was found in ./build.\n";
        return 1;
    }

    return runCommand(binary);
}

std::unordered_map<std::string, fs::file_time_type> snapshotSources() {
    std::unordered_map<std::string, fs::file_time_type> snapshot;
    for (const auto& entry : fs::recursive_directory_iterator(fs::current_path())) {
        if (!entry.is_regular_file()) {
            continue;
        }
        const auto path = entry.path();
        if (path.string().find("/build/") != std::string::npos) {
            continue;
        }
        const auto ext = path.extension().string();
        if (ext == ".cpp" || ext == ".h" || ext == ".hpp" || path.filename() == "CMakeLists.txt") {
            snapshot[path.string()] = fs::last_write_time(path);
        }
    }
    return snapshot;
}

int watch() {
    auto last = snapshotSources();
    std::cout << "Watching for changes. Press Ctrl+C to stop.\n";
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        auto current = snapshotSources();
        if (current != last) {
            last = current;
            if (build(false) != 0) {
                std::cerr << "Build failed. Waiting for changes...\n";
            }
        }
    }
}

int doctor() {
    int status = 0;
    status |= runCommand("cmake --version");
    status |= runCommand("c++ --version");
    std::cout << "\nIf Drogon is missing, install it through your system packages or vcpkg.\n";
    return status == 0 ? 0 : 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 0;
    }

    const std::string command = argv[1];
    const bool release = argc > 2 && std::string(argv[2]) == "--release";

    if (command == "create") {
        if (argc < 3) {
            std::cerr << "Missing app name.\n";
            return 1;
        }
        return createApp(argv[2]);
    }

    if (command == "build") return build(release);
    if (command == "run") return run(release);
    if (command == "watch") return watch();
    if (command == "doctor") return doctor();
    if (command == "clean") {
        fs::remove_all("build");
        std::cout << "Removed ./build\n";
        return 0;
    }

    usage();
    return 1;
}
