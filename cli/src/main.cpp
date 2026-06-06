#include <array>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace fs = std::filesystem;

// ============================================================
//  ANSI colour helpers (disabled when stdout is not a TTY)
// ============================================================
namespace colour {

inline bool ok() {
#if defined(_WIN32)
    return false;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

inline const char* red()    { return ok() ? "\033[31m" : ""; }
inline const char* green()  { return ok() ? "\033[32m" : ""; }
inline const char* yellow() { return ok() ? "\033[33m" : ""; }
inline const char* cyan()   { return ok() ? "\033[36m" : ""; }
inline const char* bold()   { return ok() ? "\033[1m"  : ""; }
inline const char* dim()    { return ok() ? "\033[2m"  : ""; }
inline const char* reset()  { return ok() ? "\033[0m"  : ""; }

} // namespace colour

// ============================================================
//  Printing helpers
// ============================================================
static void info(const std::string& msg) {
    std::cout << colour::cyan() << "  ▸ " << colour::reset() << msg << "\n";
}

static void success(const std::string& msg) {
    std::cout << colour::green() << colour::bold()
              << "  ✓ " << colour::reset() << msg << "\n";
}

static void warn(const std::string& msg) {
    std::cout << colour::yellow() << colour::bold()
              << "  ⚠ " << colour::reset() << colour::yellow() << msg
              << colour::reset() << "\n";
}

static void error(const std::string& heading,
                  const std::string& detail = "",
                  const std::string& fix    = "") {
    std::cerr << "\n"
              << colour::red() << colour::bold()
              << "  ✗ " << heading << "\n"
              << colour::reset();
    if (!detail.empty()) {
        std::cerr << colour::dim() << "    " << detail
                  << colour::reset() << "\n";
    }
    if (!fix.empty()) {
        std::cerr << "\n"
                  << colour::yellow() << colour::bold()
                  << "  → Fix: " << colour::reset()
                  << colour::yellow() << fix
                  << colour::reset() << "\n";
    }
    std::cerr << "\n";
}

static void divider() {
    std::cout << colour::dim()
              << "  ─────────────────────────────────────────\n"
              << colour::reset();
}

// ============================================================
//  usage()
// ============================================================
static void usage() {
    std::cout
        << "\n"
        << colour::cyan() << colour::bold()
        << "  Xpress++  —  Dragon-speed, Express-simplicity\n"
        << colour::reset()
        << "\n"
        << colour::bold() << "  Usage:\n" << colour::reset()
        << colour::dim()
        << "    xp create  <app-name>   Create a new Xpress++ project\n"
        << "    xp build   [--release]  Compile the current project\n"
        << "    xp run     [--release]  Build and run the server\n"
        << "    xp watch                Auto-rebuild on source changes\n"
        << "    xp clean                Delete the build directory\n"
        << "    xp doctor               Check system dependencies\n"
        << "    xp routes               List all registered routes (from build)\n"
        << colour::reset()
        << "\n";
}

// ============================================================
//  runCommand — execute a shell command and return exit code
// ============================================================
static int runCommand(const std::string& command, bool show = true) {
    if (show) {
        std::cout << colour::dim() << "  $ " << command
                  << colour::reset() << "\n";
    }
    return std::system(command.c_str());  // NOLINT
}

// Capture command output to a string (for parsing)
static std::string captureCommand(const std::string& command) {
    std::string result;
    std::array<char, 256> buf{};
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buf.data(), buf.size(), pipe) != nullptr) {
        result += buf.data();
    }
    pclose(pipe);
    return result;
}

// ============================================================
//  repoRoot() — locate the xpress++ installation directory
// ============================================================
static fs::path repoRoot() {
    // Prefer explicit env vars
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
                if (current == current.root_path()) break;
                current = current.parent_path();
            }
        }
    }

    // Walk up from cwd
    auto current = fs::current_path();
    while (!current.empty()) {
        if (fs::exists(current / "core" / "include" / "xpresspp" / "xpresspp.h") &&
            fs::exists(current / "templates" / "default_app")) {
            return current;
        }
        if (current == current.root_path()) break;
        current = current.parent_path();
    }

    return fs::current_path();
}

// ============================================================
//  isXpressProject() — check if cwd looks like an xp project
// ============================================================
static bool isXpressProject(std::string* reason = nullptr) {
    if (!fs::exists("CMakeLists.txt")) {
        if (reason) *reason = "No CMakeLists.txt found in the current directory.";
        return false;
    }
    if (!fs::exists("main.cpp")) {
        if (reason) *reason = "No main.cpp found in the current directory.";
        return false;
    }
    return true;
}

// ============================================================
//  shouldSkipTemplatePath() — filter build artifacts from copy
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

// ============================================================
//  createApp()
// ============================================================
static int createApp(const std::string& name) {
    // Validate name
    if (name.empty()) {
        error("App name cannot be empty.",
              "",
              "xp create my-app");
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
        error("Failed to write CMakeLists.txt.",
              "Check write permissions for: " + target.string());
        return 1;
    }
    cmake
        << "cmake_minimum_required(VERSION 3.16)\n"
        << "project(" << name << " CXX)\n\n"
        << "set(CMAKE_CXX_STANDARD 20)\n"
        << "set(CMAKE_CXX_STANDARD_REQUIRED ON)\n"
        << "set(CMAKE_CXX_EXTENSIONS OFF)\n\n"
        << "find_package(Drogon REQUIRED)\n"
        << "find_package(Jsoncpp REQUIRED)\n\n"
        << "add_executable(" << name << " main.cpp)\n"
        << "target_include_directories(" << name
        << " PRIVATE ${CMAKE_CURRENT_LIST_DIR}/vendor/xpresspp/include)\n"
        << "target_link_libraries(" << name << " PRIVATE Drogon::Drogon)\n\n"
        << "if(TARGET Jsoncpp_lib)\n"
        << "    target_link_libraries(" << name << " PRIVATE Jsoncpp_lib)\n"
        << "elseif(TARGET JsonCpp::JsonCpp)\n"
        << "    target_link_libraries(" << name << " PRIVATE JsonCpp::JsonCpp)\n"
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
//  build()
// ============================================================
static int build(bool release) {
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
    rc = runCommand("cmake --build build --parallel 2>&1");
    if (rc != 0) {
        // Try to give a friendly interpretation
        error("Compilation failed.",
              "See the compiler output above for the exact error.");

        // Scan build output for common patterns
        const auto build_output = captureCommand("cmake --build build 2>&1");

        if (build_output.find("error: use of undeclared identifier") != std::string::npos ||
            build_output.find("error: 'xp::") != std::string::npos) {
            warn("Looks like an xpress++ API usage error. Double-check your includes and namespace.");
        }
        if (build_output.find("undefined reference") != std::string::npos) {
            warn("Undefined reference — a library might not be linked.\n"
                 "    Check your CMakeLists.txt target_link_libraries().");
        }
        if (build_output.find("fatal error:") != std::string::npos &&
            build_output.find(".h: No such file") != std::string::npos) {
            warn("A header file was not found. Run 'xp doctor' to check your setup,\n"
                 "    and verify your vendor/xpresspp/include directory exists.");
        }
        if (build_output.find("std::") != std::string::npos &&
            build_output.find("C++20") == std::string::npos &&
            build_output.find("c++20") == std::string::npos) {
            warn("If you see C++ standard errors, ensure C++20 is set in CMakeLists.txt:\n"
                 "    set(CMAKE_CXX_STANDARD 20)");
        }
        return 1;
    }

    success("Build complete.");
    return 0;
}

// ============================================================
//  projectBinary() — find the built executable
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

// ============================================================
//  run()
// ============================================================
static int run(bool release) {
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
//  watch()
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

static int watch() {
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
//  doctor()
// ============================================================
static int doctor() {
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
            "find /usr /usr/local /opt -name 'DrogonConfig.cmake' 2>/dev/null | head -1");
        const bool found = !out.empty() && out.find('/') != std::string::npos;

        // Also try pkg-config
        const bool pkg = (std::system("pkg-config --exists drogon 2>/dev/null") == 0);

        if (found || pkg) {
            success(std::string("Drogon library  ") + colour::dim() + "(found)" + colour::reset());
        } else {
            ++problems;
            warn("Drogon — NOT FOUND");
            std::cerr
                << colour::yellow()
                << "    Fix: https://github.com/drogonframework/drogon/wiki/ENG-02-Installation\n"
                << "    Ubuntu quick: sudo apt install libdrogon-dev\n"
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
//  clean()
// ============================================================
static int clean() {
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

// ============================================================
//  main()
// ============================================================
int main(int argc, char** argv) {
    if (argc < 2) {
        usage();
        return 0;
    }

    const std::string command = argv[1];
    const bool release =
        argc > 2 && std::string(argv[2]) == "--release";

    if (command == "create") {
        if (argc < 3) {
            error("Missing app name.",
                  "",
                  "xp create <app-name>");
            return 1;
        }
        return createApp(argv[2]);
    }

    if (command == "build")  return build(release);
    if (command == "run")    return run(release);
    if (command == "watch")  return watch();
    if (command == "doctor") return doctor();
    if (command == "clean")  return clean();

    error("Unknown command: \"" + command + "\"",
          "",
          "Run 'xp' with no arguments to see available commands.");
    return 1;
}
