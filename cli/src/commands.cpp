#include "cli/commands.h"
#include "cli/diagnostics.h"
#include "cli/common.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <sstream>

namespace xp::cli {

// ============================================================
//  Create Command Helpers
// ============================================================
static bool shouldSkipTemplatePath(const fs::path& relative) {
    for (const auto& part : relative) {
        const auto name = part.string();
        if (name == "build" || name == ".git" || name == ".vscode" ||
            name == "CMakeFiles") {
            return true;
        }
    }
    const auto filename = relative.filename().string();
    if (filename == "CMakeCache.txt"      ||
        filename == "cmake_install.cmake" ||
        filename == "compile_commands.json" ||
        filename == "Makefile") {
        return true;
    }
    const auto ext = relative.extension().string();
    return ext == ".o"   || ext == ".obj" || ext == ".a"   || ext == ".so" ||
           ext == ".dylib" || ext == ".dll" || ext == ".exe";
}

static void copyDirectory(const fs::path& from, const fs::path& to) {
    fs::create_directories(to);
    for (const auto& entry : fs::recursive_directory_iterator(from)) {
        const auto relative = fs::relative(entry.path(), from);
        if (shouldSkipTemplatePath(relative)) continue;

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
    // Validate name
    if (name.empty()) {
        error("App name cannot be empty.", "", "xp create my-app");
        return 1;
    }
    for (char c : name) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '-' && c != '_') {
            error("Invalid app name: \"" + name + "\"",
                  "App names may only contain letters, numbers, hyphens, and underscores.",
                  "xp create my-app");
            return 1;
        }
    }

    const fs::path target = fs::current_path() / name;
    if (fs::exists(target)) {
        error("Cannot create app: directory already exists.",
              target.string(),
              "Choose a different name or delete the existing directory first.");
        return 1;
    }

    const fs::path root         = repoRoot();
    const fs::path template_dir = root / "templates" / "default_app";
    const fs::path core_include = root / "core" / "include";

    if (!fs::exists(template_dir)) {
        error("Xpress++ template directory not found.",
              template_dir.string(),
              "Set XPRESSPP_HOME to the Xpress++ repo path:\n"
              "    export XPRESSPP_HOME=/path/to/xpress++");
        return 1;
    }
    if (!fs::exists(core_include)) {
        error("Xpress++ core include directory not found.",
              core_include.string(),
              "Set XPRESSPP_HOME to the Xpress++ repo path.");
        return 1;
    }

    info("Creating project \"" + name + "\" ...");
    copyDirectory(template_dir, target);
    copyDirectory(core_include, target / "vendor" / "xpresspp" / "include");

    // Write CMakeLists.txt
    std::ofstream cmake(target / "CMakeLists.txt");
    if (!cmake) {
        error("Failed to write CMakeLists.txt.", "Check write permissions for: " + target.string());
        return 1;
    }
    cmake
        << "cmake_minimum_required(VERSION 3.16)\n"
        << "project(" << name << " CXX)\n\n"
        << "set(CMAKE_CXX_STANDARD 20)\n"
        << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        << "set(CMAKE_CXX_EXTENSIONS OFF)\n"
        << "set(CMAKE_EXPORT_COMPILE_COMMANDS ON)\n\n"
        << "# Try system Drogon first; fall back to downloading from GitHub.\n"
        << "find_package(Drogon QUIET)\n"
        << "if(NOT Drogon_FOUND)\n"
        << "    message(STATUS \"[" << name << "] Drogon not found — fetching from GitHub (first build only)...\")\n"
        << "    include(FetchContent)\n"
        << "    FetchContent_Declare(drogon\n"
        << "        GIT_REPOSITORY https://github.com/drogonframework/drogon.git\n"
        << "        GIT_TAG        master\n"
        << "        GIT_SHALLOW    TRUE\n"
        << "        GIT_SUBMODULES \"lib/trantor\")\n"
        << "    set(BUILD_CTL      OFF CACHE BOOL \"\" FORCE)\n"
        << "    set(BUILD_EXAMPLES OFF CACHE BOOL \"\" FORCE)\n"
        << "    set(BUILD_ORM      OFF CACHE BOOL \"\" FORCE)\n"
        << "    FetchContent_MakeAvailable(drogon)\n"
        << "endif()\n\n"
        << "add_executable(" << name << " main.cpp)\n"
        << "target_include_directories(" << name
        << " PRIVATE ${CMAKE_CURRENT_LIST_DIR}/vendor/xpresspp/include)\n"
        << "target_link_libraries(" << name << " PRIVATE Drogon::Drogon)\n\n"
        << "if(TARGET jsoncpp_lib)\n"
        << "    target_link_libraries(" << name << " PRIVATE jsoncpp_lib)\n"
        << "elseif(TARGET JsonCpp::JsonCpp)\n"
        << "    target_link_libraries(" << name << " PRIVATE JsonCpp::JsonCpp)\n"
        << "elseif(TARGET Jsoncpp_lib)\n"
        << "    target_link_libraries(" << name << " PRIVATE Jsoncpp_lib)\n"
        << "else()\n"
        << "    target_include_directories(" << name
        << " PRIVATE /usr/include/jsoncpp)\n"
        << "    target_link_libraries(" << name << " PRIVATE jsoncpp)\n"
        << "endif()\n";

    // Write .gitignore
    std::ofstream gitignore(target / ".gitignore");
    gitignore << "build/\n.vscode/\nuploads/tmp/\n";

    divider();
    success(std::string("Created project  ") + colour::bold() + name + colour::reset());
    std::cout << "\n"
              << colour::bold() << "  Next steps:\n" << colour::reset()
              << colour::cyan()
              << "    cd " << name << "\n"
              << "    xp run\n"
              << colour::reset()
              << "\n";
    return 0;
}

// ============================================================
//  Build Command
// ============================================================
int build(bool release) {
    std::string reason;
    if (!isXpressProject(&reason)) {
        error("Not an Xpress++ project directory.",
              reason,
              "Run this command from inside your project folder (where main.cpp lives).");
        return 1;
    }

    const std::string type = release ? "Release" : "Debug";

    // Detect stale cache from a different project
    const fs::path cache = fs::current_path() / "build" / "CMakeCache.txt";
    if (fs::exists(cache)) {
        std::ifstream input(cache);
        std::string   line;
        while (std::getline(input, line)) {
            if (line.rfind("CMAKE_HOME_DIRECTORY:INTERNAL=", 0) == 0) {
                const auto cached_source = fs::path(line.substr(30));
                if (fs::weakly_canonical(cached_source) !=
                    fs::weakly_canonical(fs::current_path())) {
                    warn("Stale CMake cache detected (from a different project). Cleaning...");
                    fs::remove_all("build");
                }
                break;
            }
        }
    }

    info("Configuring with CMake (" + type + " mode) ...");
    int rc = runCommand("cmake -S . -B build -DCMAKE_BUILD_TYPE=" + type +
                        " -DCMAKE_EXPORT_COMPILE_COMMANDS=ON 2>&1");
    if (rc != 0) {
        error("CMake configuration failed.",
              "See the output above for details.",
              "Common fixes:\n"
              "    • Install Drogon:  https://github.com/drogonframework/drogon/wiki/ENG-02-Installation\n"
              "    • Install Jsoncpp: sudo apt install libjsoncpp-dev  (Ubuntu/Debian)\n"
              "    • Ensure cmake >= 3.16 is installed: cmake --version");
        return 1;
    }

    info("Compiling ...");
    std::string build_output;
    rc = runBuildAndFilter("cmake --build build --parallel 2>&1", build_output);
    if (rc != 0) {
        error("Compilation failed.");
        explainCompilerErrors(build_output);
        return 1;
    }

    success("Build complete.");
    return 0;
}

// ============================================================
//  Run Command Helpers & Implementation
// ============================================================
static std::string projectBinary() {
    if (!fs::exists("build")) return "";

    fs::path best;
    for (const auto& entry : fs::directory_iterator("build")) {
        if (!entry.is_regular_file()) continue;
        const auto perms = entry.status().permissions();
        const bool exec  =
            (perms & fs::perms::owner_exec)  != fs::perms::none ||
            (perms & fs::perms::group_exec)  != fs::perms::none ||
            (perms & fs::perms::others_exec) != fs::perms::none;
        if (exec && entry.path().filename() != "cmake_install.cmake") {
            best = entry.path();
        }
    }
    return best.string();
}

int run(bool release) {
    if (build(release) != 0) return 1;

    const auto binary = projectBinary();
    if (binary.empty()) {
        error("Build succeeded but no executable found in ./build.",
              "This is unexpected — the build system may have placed it elsewhere.",
              "Try: ls -la build/  to inspect the directory.");
        return 1;
    }

    info("Starting server: " + binary);
    divider();
    return runCommand(binary);
}

// ============================================================
//  Watch Command Helpers & Implementation
// ============================================================
static std::unordered_map<std::string, fs::file_time_type> snapshotSources() {
    std::unordered_map<std::string, fs::file_time_type> snapshot;
    try {
        for (const auto& entry : fs::recursive_directory_iterator(fs::current_path())) {
            if (!entry.is_regular_file()) continue;
            const auto path = entry.path();
            if (path.string().find("/build/") != std::string::npos) continue;
            const auto ext = path.extension().string();
            if (ext == ".cpp" || ext == ".h" || ext == ".hpp" ||
                path.filename() == "CMakeLists.txt") {
                snapshot[path.string()] = fs::last_write_time(path);
            }
        }
    } catch (const fs::filesystem_error& e) {
        warn("watch: could not read some source files: " + std::string(e.what()));
    }
    return snapshot;
}

int watch() {
    std::string reason;
    if (!isXpressProject(&reason)) {
        error("Not an Xpress++ project directory.", reason,
              "Run this command from inside your project folder.");
        return 1;
    }

    auto last = snapshotSources();
    info("Watching for source changes. Press Ctrl+C to stop.");
    divider();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        auto current = snapshotSources();
        if (current != last) {
            last = current;
            info("Changes detected — rebuilding ...");
            if (build(false) != 0) {
                warn("Build failed. Fix the errors above and save your files to retry.");
            } else {
                success("Rebuild complete.");
            }
        }
    }
}

// ============================================================
//  Doctor Command
// ============================================================
int doctor() {
    std::cout
        << "\n"
        << colour::cyan() << colour::bold()
        << "  Xpress++ System Check\n"
        << colour::reset();
    divider();

    int problems = 0;

    auto check = [&](const std::string& label,
                     const std::string& command,
                     const std::string& fix) {
        const auto output = captureCommand(command + " 2>&1");
        const bool ok     = !output.empty() && output.find("not found") == std::string::npos;
        if (ok) {
            // Extract first line of version output
            std::istringstream ss(output);
            std::string first_line;
            std::getline(ss, first_line);
            success(label + "  " + colour::dim() + first_line + colour::reset());
        } else {
            ++problems;
            warn(label + " — NOT FOUND");
            std::cerr
                << colour::yellow()
                << "    Fix: " << fix
                << colour::reset() << "\n";
        }
    };

    // --- cmake ---
    check("cmake",
          "cmake --version",
          "Install cmake >= 3.16.  Ubuntu: sudo apt install cmake");

    // --- C++ compiler ---
    {
        const auto out = captureCommand("c++ --version 2>&1");
        if (!out.empty()) {
            std::istringstream ss(out);
            std::string first;
            std::getline(ss, first);
            success(std::string("C++ compiler  ") + colour::dim() + first + colour::reset());
        } else {
            ++problems;
            warn("C++ compiler — NOT FOUND");
            std::cerr << colour::yellow()
                      << "    Fix: sudo apt install g++ build-essential\n"
                      << colour::reset();
        }
    }

    // --- Drogon ---
    {
        const auto out = captureCommand(
            "find /usr /usr/local /opt/homebrew -name 'DrogonConfig.cmake' 2>/dev/null | head -1");
        const bool found = !out.empty() && out.find('/') != std::string::npos;
        const bool pkg   = (std::system("pkg-config --exists drogon 2>/dev/null") == 0);

        if (found || pkg) {
            success(std::string("Drogon library  ") + colour::dim() + "(found)" + colour::reset());
        } else {
            ++problems;
            warn("Drogon — NOT FOUND");
            std::cerr
                << colour::yellow()
                << "    Note: FetchContent in your CMakeLists.txt will download Drogon\n"
                << "          automatically on the first 'xp build' — no manual install needed.\n"
                << "    Or install manually:  ./install.sh\n"
                << "    Or run:               xp doctor --fix\n"
                << colour::reset();
        }
    }

    // --- jsoncpp ---
    {
        const auto out = captureCommand(
            "find /usr /usr/local -name 'json.h' -path '*/jsoncpp/*' 2>/dev/null | head -1");
        const bool found = !out.empty();
        if (found) {
            success(std::string("jsoncpp  ") + colour::dim() + "(found)" + colour::reset());
        } else {
            ++problems;
            warn("jsoncpp — NOT FOUND");
            std::cerr
                << colour::yellow()
                << "    Fix: sudo apt install libjsoncpp-dev\n"
                << colour::reset();
        }
    }

    // --- OpenSSL (optional, for TLS) ---
    {
        const auto out = captureCommand("openssl version 2>&1");
        if (!out.empty() && out.find("OpenSSL") != std::string::npos) {
            std::istringstream ss(out); std::string first; std::getline(ss, first);
            success(std::string("OpenSSL (TLS)  ") + colour::dim() + first + colour::reset());
        } else {
            std::cout << colour::dim()
                      << "  - OpenSSL — not found (optional, only needed for HTTPS)\n"
                      << colour::reset();
        }
    }

    // --- xpress++ repo root ---
    {
        const auto root = repoRoot();
        const bool valid = fs::exists(root / "core" / "include" / "xpresspp" / "xpresspp.h");
        if (valid) {
            success(std::string("Xpress++ headers  ") + colour::dim() + root.string() + colour::reset());
        } else {
            ++problems;
            warn("Xpress++ headers — could not locate repo root");
            std::cerr
                << colour::yellow()
                << "    Fix: export XPRESSPP_HOME=/path/to/xpress++\n"
                << colour::reset();
        }
    }

    divider();
    if (problems == 0) {
        success("All checks passed — you are ready to build with Xpress++!");
    } else {
        std::cout
            << colour::red() << colour::bold()
            << "  " << problems << " problem(s) found."
            << colour::reset() << "\n"
            << colour::yellow()
            << "  Fix the issues above, then run 'xp doctor' again.\n"
            << colour::reset();
    }
    std::cout << "\n";
    return problems == 0 ? 0 : 1;
}

// ============================================================
//  Install Command
// ============================================================
int installDeps() {
    std::cout << "\n"
              << colour::cyan() << colour::bold()
              << "  Xpress++ — Installing Dependencies\n"
              << colour::reset();
    divider();

    // Detect package manager
    bool hasApt    = (std::system("command -v apt-get >/dev/null 2>&1") == 0);
    bool hasPacman = (std::system("command -v pacman  >/dev/null 2>&1") == 0);
    bool hasDnf    = (std::system("command -v dnf    >/dev/null 2>&1") == 0);
    bool hasBrew   = (std::system("command -v brew   >/dev/null 2>&1") == 0);

    if (!hasApt && !hasPacman && !hasDnf && !hasBrew) {
        error("Could not detect a supported package manager.",
              "Tried: apt-get, pacman, dnf, brew",
              "Please install Drogon manually:\n"
              "    https://github.com/drogonframework/drogon/wiki/ENG-02-Installation");
        return 1;
    }

    if (hasApt) {
        info("Using apt-get (Ubuntu/Debian)...");
        std::system("sudo apt-get update -qq");
        int rc = runCommand(
            "sudo apt-get install -y build-essential cmake git "
            "libssl-dev libjsoncpp-dev zlib1g-dev uuid-dev");
        if (rc != 0) {
            error("apt-get install failed.",
                  "Try running the command manually with sudo.");
            return 1;
        }
    } else if (hasPacman) {
        info("Using pacman (Arch Linux)...");
        int rc = runCommand(
            "sudo pacman -Sy --noconfirm base-devel cmake git jsoncpp openssl zlib uuid");
        if (rc != 0) { error("pacman install failed."); return 1; }
    } else if (hasDnf) {
        info("Using dnf (Fedora/RHEL)...");
        int rc = runCommand(
            "sudo dnf install -y gcc-c++ cmake git openssl-devel jsoncpp-devel zlib-devel");
        if (rc != 0) { error("dnf install failed."); return 1; }
    } else if (hasBrew) {
        info("Using Homebrew (macOS)...");
        int rc = runCommand("brew install cmake jsoncpp openssl zlib");
        if (rc != 0) { error("brew install failed."); return 1; }
    }

    // Check if Drogon is already installed
    const auto drogonCheck = captureCommand(
        "find /usr /usr/local /opt/homebrew -name 'DrogonConfig.cmake' 2>/dev/null | head -1");
    const bool drogonFound = !drogonCheck.empty() && drogonCheck.find('/') != std::string::npos;

    if (drogonFound) {
        success("Drogon is already installed.");
    } else {
        info("Building Drogon from source (this takes a few minutes on first run)...");
        int rc = runCommand(
            "git clone --depth 1 --recurse-submodules "
            "https://github.com/drogonframework/drogon /tmp/xpresspp_drogon_build 2>&1 || "
            "echo 'already cloned'");
        if (rc == 0) {
            rc = runCommand(
                "cmake -S /tmp/xpresspp_drogon_build -B /tmp/xpresspp_drogon_build/build "
                "-DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF "
                "-DBUILD_CTL=OFF -DBUILD_ORM=OFF 2>&1");
        }
        if (rc == 0) {
            rc = runCommand("cmake --build /tmp/xpresspp_drogon_build/build --parallel 2>&1");
        }
        if (rc == 0) {
            rc = runCommand("sudo cmake --install /tmp/xpresspp_drogon_build/build 2>&1");
        }
        if (rc != 0) {
            error("Drogon build failed.",
                  "Check the output above for details.",
                  "You can also let CMake download it automatically:\n"
                  "    xp build  (FetchContent handles it on first compile)");
            return 1;
        }
        success("Drogon installed.");
    }

    divider();
    success("All dependencies installed! Run 'xp doctor' to verify.");
    std::cout << "\n";
    return 0;
}

// ============================================================
//  Clean Command
// ============================================================
int clean() {
    if (!fs::exists("build")) {
        info("Nothing to clean (./build does not exist).");
        return 0;
    }
    std::error_code ec;
    fs::remove_all("build", ec);
    if (ec) {
        error("Failed to remove ./build", ec.message(),
              "Check permissions: ls -la build/");
        return 1;
    }
    success("Removed ./build");
    return 0;
}

} // namespace xp::cli
