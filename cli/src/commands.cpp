#include "cli/commands.h"
#include "cli/diagnostics.h"
#include "cli/common.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <sstream>
#include <algorithm>
#include <cctype>

#if !defined(_WIN32)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

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
        << "# Set XPRESSPP_HOME path to find core headers\n"
        << "set(XPRESSPP_HOME \"" << root.string() << "\")\n\n"
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
        << "target_include_directories(" << name << " PRIVATE\n"
        << "    ${XPRESSPP_HOME}/core/include\n"
        << "    ${CMAKE_CURRENT_LIST_DIR}/build/generated)\n"
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
    gitignore
        << "build/\n"
        << ".vscode/\n"
        << ".env\n"          // never commit secrets
        << "*.db\n"
        << "*.log\n";

    // Write .vscode/c_cpp_properties.json to silence C++ IntelliSense squiggle errors
    try {
        fs::create_directories(target / ".vscode");
        std::ofstream c_cpp_properties(target / ".vscode" / "c_cpp_properties.json");
        if (c_cpp_properties) {
            c_cpp_properties
                << "{\n"
                << "    \"configurations\": [\n"
                << "        {\n"
                << "            \"name\": \"Linux\",\n"
                << "            \"includePath\": [\n"
                << "                \"${workspaceFolder}/**\",\n"
                << "                \"" << (root / "core" / "include").string() << "\",\n"
                << "                \"/usr/include/jsoncpp\"\n"
                << "            ],\n"
                << "            \"defines\": [],\n"
                << "            \"compilerPath\": \"/usr/bin/gcc\",\n"
                << "            \"cStandard\": \"c17\",\n"
                << "            \"cppStandard\": \"gnu++20\",\n"
                << "            \"intelliSenseMode\": \"linux-gcc-x64\",\n"
                << "            \"compileCommands\": \"${workspaceFolder}/build/compile_commands.json\"\n"
                << "        }\n"
                << "    ],\n"
                << "    \"version\": 4\n"
                << "}\n";
        }
    } catch (...) {}

    divider();
    success(std::string("Created project  ") + colour::bold() + name + colour::reset());
    std::cout
        << "\n"
        << colour::bold() << "  Next steps:\n" << colour::reset()
        << colour::cyan()
        << "    cd " << name << "\n"
        << "    cp .env.example .env      # set your config\n"
        << "    xp dev                    # build, run, and live-reload\n"
        << colour::reset()
        << "\n"
        << colour::dim()
        << "  Or for a one-shot run:\n"
        << "    xp run\n"
        << colour::reset()
        << "\n";
    return 0;
}

// ============================================================
//  Generate Command
// ============================================================
static std::string toIdentifier(std::string value) {
    for (auto& c : value) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            c = '_';
        }
    }
    if (value.empty() || std::isdigit(static_cast<unsigned char>(value.front()))) {
        value = "_" + value;
    }
    return value;
}

static std::string toPascal(std::string value) {
    std::string result;
    bool upper_next = true;
    for (char c : value) {
        if (!std::isalnum(static_cast<unsigned char>(c))) {
            upper_next = true;
            continue;
        }
        if (upper_next) {
            result.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(c))));
            upper_next = false;
        } else {
            result.push_back(c);
        }
    }
    if (result.empty()) result = "Generated";
    if (std::isdigit(static_cast<unsigned char>(result.front()))) {
        result = "Generated" + result;
    }
    return result;
}

static std::string toSnake(std::string value) {
    std::string result;
    bool previous_sep = false;
    for (char c : value) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            result.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
            previous_sep = false;
        } else if (!previous_sep && !result.empty()) {
            result.push_back('_');
            previous_sep = true;
        }
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    return result.empty() ? "generated" : result;
}

static int writeGeneratedFile(const fs::path& path, const std::string& contents) {
    if (fs::exists(path)) {
        error("Refusing to overwrite existing file.", path.string(),
              "Choose a different name or edit the existing file.");
        return 1;
    }

    fs::create_directories(path.parent_path());
    std::ofstream output(path);
    if (!output) {
        error("Failed to write generated file.", path.string(),
              "Check write permissions for this project.");
        return 1;
    }
    output << contents;
    success("Created " + path.string());
    return 0;
}

int generate(const std::string& raw_type, const std::string& raw_name) {
    std::string reason;
    if (!isXpressProject(&reason)) {
        error("Not an Xpress++ project directory.", reason,
              "Run this command from inside your project folder.");
        return 1;
    }

    std::string type = raw_type;
    std::transform(type.begin(), type.end(), type.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });

    const auto snake = toSnake(raw_name);
    const auto ident = toIdentifier(snake);
    const auto pascal = toPascal(raw_name);

    if (type == "route" || type == "routes") {
        const fs::path path = fs::path("src") / "routes" / (snake + ".h");
        std::ostringstream body;
        body
            << "#pragma once\n\n"
            << "#include <xpresspp/xpresspp.h>\n\n"
            << "inline xp::Router " << ident << "Routes() {\n"
            << "    xp::Router router;\n\n"
            << "    router.get(\"/\", [](xp::Request& req, xp::Response& res) {\n"
            << "        res.ok({{\"resource\", \"" << snake << "\"}});\n"
            << "    });\n\n"
            << "    router.post(\"/\", [](xp::Request& req, xp::Response& res) {\n"
            << "        const auto body = req.json();\n"
            << "        res.created({{\"created\", true}, {\"data\", body}});\n"
            << "    });\n\n"
            << "    return router;\n"
            << "}\n";
        const int rc = writeGeneratedFile(path, body.str());
        if (rc == 0) {
            std::cout << colour::dim()
                      << "  Mount it with: app.use(\"/" << snake << "\", " << ident << "Routes());\n"
                      << colour::reset();
        }
        return rc;
    }

    if (type == "controller" || type == "controllers") {
        const fs::path path = fs::path("src") / "controllers" / (snake + "_controller.h");
        std::ostringstream body;
        body
            << "#pragma once\n\n"
            << "#include <xpresspp/xpresspp.h>\n\n"
            << "struct " << pascal << "Controller {\n"
            << "    static void index(xp::Request& req, xp::Response& res) {\n"
            << "        res.ok({{\"resource\", \"" << snake << "\"}});\n"
            << "    }\n\n"
            << "    static void show(xp::Request& req, xp::Response& res) {\n"
            << "        res.ok({{\"id\", req.param(\"id\", true)}});\n"
            << "    }\n"
            << "};\n";
        return writeGeneratedFile(path, body.str());
    }

    if (type == "middleware" || type == "mw") {
        const fs::path path = fs::path("src") / "middleware" / (snake + ".h");
        std::ostringstream body;
        body
            << "#pragma once\n\n"
            << "#include <xpresspp/xpresspp.h>\n\n"
            << "inline xp::Middleware " << ident << "() {\n"
            << "    return [](xp::Request& req, xp::Response& res, xp::Next next) {\n"
            << "        next();\n"
            << "    };\n"
            << "}\n";
        return writeGeneratedFile(path, body.str());
    }

    if (type == "model" || type == "models") {
        const fs::path path = fs::path("src") / "models" / (snake + ".h");
        std::ostringstream body;
        body
            << "#pragma once\n\n"
            << "#include <json/json.h>\n"
            << "#include <string>\n\n"
            << "struct " << pascal << " {\n"
            << "    int id = 0;\n"
            << "    std::string name;\n\n"
            << "    Json::Value toJson() const {\n"
            << "        Json::Value value;\n"
            << "        value[\"id\"] = id;\n"
            << "        value[\"name\"] = name;\n"
            << "        return value;\n"
            << "    }\n"
            << "};\n";
        return writeGeneratedFile(path, body.str());
    }

    error("Unknown generator type: \"" + raw_type + "\"",
          "Supported types: route, controller, middleware, model.",
          "xp generate route users\n"
          "xp g middleware auth");
    return 1;
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

    // Initial build
    info("Initial build before watching...");
    if (build(false) != 0) {
        warn("Initial build failed. Fix the errors above — watching for changes to retry.");
    }

    auto last = snapshotSources();
    info("Watching for source changes. Press Ctrl+C to stop.");
    divider();

#if defined(_WIN32)
    // Windows: basic watch without process tracking
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));
        auto current = snapshotSources();
        if (current != last) {
            last = current;
            info("Changes detected — rebuilding ...");
            if (build(false) != 0) {
                warn("Build failed. Fix the errors above and save your files to retry.");
            } else {
                success("Rebuild complete. Restart the server manually on Windows.");
            }
        }
    }
#else
    pid_t server_pid = -1;

    // Helper: start the server binary as a child process
    auto startServer = [&]() {
        const auto binary = projectBinary();
        if (binary.empty()) {
            warn("No executable found in ./build — skipping server launch.");
            return;
        }
        server_pid = ::fork();
        if (server_pid == 0) {
            // Child: exec the server
            ::execl(binary.c_str(), binary.c_str(), nullptr);
            ::_exit(127); // exec failed
        } else if (server_pid < 0) {
            warn("fork() failed — could not launch server.");
            server_pid = -1;
        } else {
            info("Server started (PID " + std::to_string(server_pid) + "): " + binary);
        }
    };

    // Helper: kill the current server process
    auto stopServer = [&]() {
        if (server_pid > 0) {
            info("Stopping server (PID " + std::to_string(server_pid) + ") ...");
            ::kill(server_pid, SIGTERM);
            int status = 0;
            ::waitpid(server_pid, &status, 0);
            server_pid = -1;
        }
    };

    startServer();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(800));

        // Collect the child exit status without blocking (reap crashed servers)
        if (server_pid > 0) {
            int status = 0;
            pid_t result = ::waitpid(server_pid, &status, WNOHANG);
            if (result == server_pid) {
                warn("Server exited unexpectedly. Waiting for file change to restart...");
                server_pid = -1;
            }
        }

        auto current = snapshotSources();
        if (current != last) {
            last = current;
            info("Changes detected — rebuilding ...");
            stopServer();
            if (build(false) != 0) {
                warn("Build failed. Fix the errors above and save your files to retry.");
            } else {
                success("Rebuild complete — restarting server...");
                startServer();
            }
        }
    }
#endif
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

// ============================================================
//  Migrate Command (Xpress++ Database Schema Parser and Generator)
// ============================================================
struct FieldInfo {
    std::string name;
    std::string type;
    bool nullable = false;
    bool isPrimaryKey = false;
    bool isUnique = false;
    bool isDefaultNow = false;
    bool isRelation = false;
    bool isList = false;
    std::string relationModel;
    std::string relationFields;
    std::string relationReferences;
};

struct ModelInfo {
    std::string name;
    std::string tableName;
    std::vector<FieldInfo> fields;
};

static std::string trimString(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

static bool isPrimitiveType(const std::string& type) {
    std::string t = type;
    if (t.size() > 2 && t.substr(t.size() - 2) == "[]") {
        t = t.substr(0, t.size() - 2);
    }
    return (t == "Int" || t == "String" || t == "Boolean" || t == "Float" || t == "DateTime");
}

static std::vector<std::string> splitString(const std::string& str) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    while (tokenStream >> token) {
        tokens.push_back(token);
    }
    return tokens;
}

int migrate(const std::string& arg1, const std::string& arg2) {
    if (arg1 == "rollback") {
        info("Rebuilding and running database migrations rollback...");
        if (build(false) != 0) {
            error("Failed to build the project for rollback.");
            return 1;
        }

        const auto binary = projectBinary();
        if (binary.empty()) {
            error("No executable found in ./build for rollback.");
            return 1;
        }

        info("Running rollback: " + binary + " --rollback");
        divider();
        return runCommand(binary + " --rollback");
    }

    std::string reason;
    if (!isXpressProject(&reason)) {
        error("Not an Xpress++ project directory.", reason,
              "Run this command from inside your project folder.");
        return 1;
    }

    const fs::path schemaPath = fs::current_path() / "schema.xp";
    if (!fs::exists(schemaPath)) {
        error("schema.xp not found.",
              "Please create a schema.xp file in the root of your project.",
              "For example:\n"
              "  model User {\n"
              "    id Int @id @default(autoincrement())\n"
              "    username String @unique\n"
              "  }");
        return 1;
    }

    info("Parsing schema.xp...");
    std::ifstream file(schemaPath);
    if (!file.is_open()) {
        error("Failed to open schema.xp", "Check read permissions on " + schemaPath.string());
        return 1;
    }

    std::string dbProvider = "postgresql"; // default provider
    std::vector<ModelInfo> models;
    ModelInfo currentModel;
    bool inModel = false;
    std::string line;

    while (std::getline(file, line)) {
        line = trimString(line);
        if (line.empty() || line.rfind("//", 0) == 0) {
            continue;
        }

        if (line.find("provider") != std::string::npos && line.find("=") != std::string::npos) {
            auto firstQuote = line.find('"');
            auto lastQuote = line.rfind('"');
            if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote < lastQuote) {
                dbProvider = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
            }
        }

        if (line.rfind("model ", 0) == 0) {
            auto tokens = splitString(line);
            if (tokens.size() >= 2) {
                currentModel = ModelInfo();
                currentModel.name = tokens[1];
                std::string lowerName = tokens[1];
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
                currentModel.tableName = lowerName + "s";
                inModel = true;
            }
            continue;
        }

        if (inModel && line == "}") {
            models.push_back(currentModel);
            inModel = false;
            continue;
        }

        if (inModel) {
            if (line.rfind("@@map(", 0) == 0) {
                auto firstQuote = line.find('"');
                auto lastQuote = line.rfind('"');
                if (firstQuote != std::string::npos && lastQuote != std::string::npos && firstQuote < lastQuote) {
                    currentModel.tableName = line.substr(firstQuote + 1, lastQuote - firstQuote - 1);
                }
                continue;
            }

            auto tokens = splitString(line);
            if (tokens.size() >= 2) {
                FieldInfo f;
                f.name = tokens[0];
                std::string typeToken = tokens[1];

                if (typeToken.back() == '?') {
                    f.nullable = true;
                    typeToken.pop_back();
                }
                f.type = typeToken;

                f.isRelation = !isPrimitiveType(typeToken);
                if (f.isRelation) {
                    if (f.type.size() > 2 && f.type.substr(f.type.size() - 2) == "[]") {
                        f.isList = true;
                        f.relationModel = f.type.substr(0, f.type.size() - 2);
                    } else {
                        f.isList = false;
                        f.relationModel = f.type;
                    }

                    auto relPos = line.find("@relation(");
                    if (relPos != std::string::npos) {
                        auto endPos = line.find(")", relPos);
                        if (endPos != std::string::npos) {
                            std::string relContent = line.substr(relPos + 10, endPos - relPos - 10);
                            auto fieldsPos = relContent.find("fields: [");
                            if (fieldsPos != std::string::npos) {
                                auto fieldsEnd = relContent.find("]", fieldsPos);
                                if (fieldsEnd != std::string::npos) {
                                    f.relationFields = relContent.substr(fieldsPos + 9, fieldsEnd - fieldsPos - 9);
                                }
                            }
                            auto refPos = relContent.find("references: [");
                            if (refPos != std::string::npos) {
                                auto refEnd = relContent.find("]", refPos);
                                if (refEnd != std::string::npos) {
                                    f.relationReferences = relContent.substr(refPos + 13, refEnd - refPos - 13);
                                }
                            }
                        }
                    }
                }

                for (size_t i = 2; i < tokens.size(); ++i) {
                    std::string dec = tokens[i];
                    if (dec == "@id") {
                        f.isPrimaryKey = true;
                    } else if (dec == "@unique") {
                        f.isUnique = true;
                    } else if (dec.rfind("@default(", 0) == 0) {
                        if (dec.find("autoincrement()") != std::string::npos) {
                            f.type = "Serial";
                            f.isPrimaryKey = true;
                        } else if (dec.find("now()") != std::string::npos) {
                            f.isDefaultNow = true;
                        }
                    }
                }

                currentModel.fields.push_back(f);
            }
        }
    }

    // Pre-resolve inverse relation fields
    for (auto& model : models) {
        for (auto& f : model.fields) {
            if (!f.isRelation) continue;
            if (f.relationFields.empty()) {
                for (const auto& targetModel : models) {
                    if (targetModel.name == f.relationModel) {
                        for (const auto& tf : targetModel.fields) {
                            if (tf.isRelation && tf.relationModel == model.name && !tf.relationFields.empty()) {
                                f.relationFields = tf.relationReferences;
                                f.relationReferences = tf.relationFields;
                                break;
                            }
                        }
                        break;
                    }
                }
            }
        }
    }

    // Generate up.sql and down.sql migrations
    std::ostringstream up_sql;
    std::ostringstream down_sql;

    for (const auto& model : models) {
        up_sql << "CREATE TABLE IF NOT EXISTS " << model.tableName << " (\n";
        bool first = true;
        for (const auto& f : model.fields) {
            if (f.isRelation) continue;
            if (!first) up_sql << ",\n";
            first = false;

            std::string sql_type;
            if (f.type == "Serial") {
                if (dbProvider == "sqlite" || dbProvider == "sqlite3") {
                    sql_type = "INTEGER PRIMARY KEY AUTOINCREMENT";
                } else {
                    sql_type = "SERIAL PRIMARY KEY";
                }
            } else if (f.type == "Int") {
                sql_type = "INTEGER";
            } else if (f.type == "Float") {
                sql_type = "REAL";
            } else if (f.type == "Boolean") {
                sql_type = "BOOLEAN";
            } else if (f.type == "DateTime") {
                sql_type = "TIMESTAMP";
            } else {
                sql_type = "TEXT";
            }

            std::string opts = "";
            if (f.isPrimaryKey && f.type != "Serial") {
                opts += " PRIMARY KEY";
            }
            if (!f.nullable && !f.isPrimaryKey && f.type != "Serial") {
                opts += " NOT NULL";
            }
            if (f.isUnique && !f.isPrimaryKey) {
                opts += " UNIQUE";
            }
            if (f.isDefaultNow) {
                if (dbProvider == "sqlite" || dbProvider == "sqlite3") {
                    opts += " DEFAULT CURRENT_TIMESTAMP";
                } else {
                    opts += " DEFAULT NOW()";
                }
            }

            up_sql << "  " << f.name << " " << sql_type << opts;
        }
        up_sql << "\n);\n\n";

        down_sql << "DROP TABLE IF EXISTS " << model.tableName << ";\n";
    }

    // Save migration files
    std::string migration_name = "migration";
    if (!arg1.empty() && arg1 != "dev" && arg1 != "--name") {
        migration_name = arg1;
    } else if (!arg2.empty() && arg2 != "--name") {
        migration_name = arg2;
    }

    std::time_t t = std::time(nullptr);
    std::tm tm = *std::localtime(&t);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%Y%m%d%H%M%S", &tm);
    std::string timestamp(buf);

    std::string migration_dir_name = timestamp + "_" + migration_name;
    fs::path migration_path = fs::current_path() / "migrations" / migration_dir_name;
    fs::create_directories(migration_path);

    std::ofstream up_file(migration_path / "up.sql");
    up_file << up_sql.str();
    up_file.close();

    std::ofstream down_file(migration_path / "down.sql");
    down_file << down_sql.str();
    down_file.close();

    success("Generated migration files in migrations/" + migration_dir_name);

    fs::path modelsDir = fs::current_path() / "build" / "generated";
    fs::create_directories(modelsDir);
    info("Generating unified C++ DB client in build/generated/db.h ...");

    // Clean up old individual files if they exist to prevent clutter
    for (const auto& model : models) {
        std::string filename = model.name;
        std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);
        fs::remove(modelsDir / (filename + ".h"));
    }
    fs::remove(modelsDir / "schema_sync.h");

    fs::path dbHeaderPath = modelsDir / "db.h";
    std::ofstream dbOut(dbHeaderPath);
    if (!dbOut.is_open()) {
        error("Failed to write db.h: " + dbHeaderPath.string());
        return 1;
    }

    dbOut << "#pragma once\n";
    dbOut << "#include <xpresspp/xpresspp.h>\n";
    dbOut << "#include <string>\n";
    dbOut << "#include <vector>\n\n";

    // 1. Output C++ Model Classes
    for (const auto& model : models) {
        if (dbProvider == "mongodb") {
            dbOut << "class " << model.name << " : public xp::MongoModel<" << model.name << "> {\n";
            dbOut << "public:\n";
            dbOut << "    using xp::MongoModel<" << model.name << ">::create;\n\n";
        } else {
            dbOut << "class " << model.name << " : public xp::Model<" << model.name << "> {\n";
            dbOut << "public:\n";
            dbOut << "    using xp::Model<" << model.name << ">::create;\n\n";
        }
        dbOut << "    static std::string tableName() { return \"" << model.tableName << "\"; }\n\n";
        dbOut << "    static xp::Schema schema() {\n";
        dbOut << "        return {\n";

        bool firstField = true;
        for (const auto& f : model.fields) {
            if (f.isRelation) continue;

            if (!firstField) dbOut << ",\n";
            firstField = false;

            std::string cppType = "xp::FieldType::Text";
            if (f.type == "Serial") cppType = "xp::FieldType::Serial";
            else if (f.type == "Int") cppType = "xp::FieldType::Integer";
            else if (f.type == "Boolean") cppType = "xp::FieldType::Boolean";
            else if (f.type == "Float") cppType = "xp::FieldType::Double";
            else if (f.type == "DateTime") cppType = "xp::FieldType::Timestamp";

            std::string options = "xp::FieldOption::None";
            if (f.isPrimaryKey) {
                options = "xp::FieldOption::PrimaryKey";
            } else {
                std::vector<std::string> opts;
                if (!f.nullable) opts.push_back("xp::FieldOption::NotNull");
                if (f.isUnique) opts.push_back("xp::FieldOption::Unique");
                if (f.isDefaultNow) opts.push_back("xp::FieldOption::DefaultNow");

                if (!opts.empty()) {
                    options = opts[0];
                    for (size_t i = 1; i < opts.size(); ++i) {
                        options += " | " + opts[i];
                    }
                }
            }

            dbOut << "            {\"" << f.name << "\", " << cppType << ", " << options << "}";
        }
        dbOut << "\n        };\n";
        dbOut << "    }\n\n";

        // Generate findBy[FieldName] for Unique / PrimaryKey fields
        for (const auto& f : model.fields) {
            if (f.isRelation) continue;
            if (f.isUnique || f.isPrimaryKey) {
                std::string capName = f.name;
                if (!capName.empty()) capName[0] = std::toupper(capName[0]);

                std::string paramType = "const std::string&";
                if (f.type == "Serial" || f.type == "Int") paramType = "int64_t";
                else if (f.type == "Float") paramType = "double";
                else if (f.type == "Boolean") paramType = "bool";

                dbOut << "    static drogon::Task<Json::Value> findBy" << capName << "(" << paramType << " val) {\n";
                dbOut << "        Json::Value where;\n";
                dbOut << "        where[\"" << f.name << "\"] = val;\n";
                dbOut << "        co_return co_await findUnique(where);\n";
                dbOut << "    }\n\n";
            }
        }

        // Generate user create shortcut helper if model has username & password fields
        bool hasUsername = false;
        bool hasPassword = false;
        for (const auto& f : model.fields) {
            if (f.name == "username") hasUsername = true;
            if (f.name == "password") hasPassword = true;
        }
        if (hasUsername && hasPassword) {
            dbOut << "    static drogon::Task<void> create(const std::string& username, const std::string& password) {\n";
            dbOut << "        Json::Value data;\n";
            dbOut << "        data[\"username\"] = username;\n";
            dbOut << "        data[\"password\"] = password;\n";
            if (dbProvider == "mongodb") {
                dbOut << "        co_await xp::MongoModel<" << model.name << ">::create(data);\n";
            } else {
                dbOut << "        co_await xp::Model<" << model.name << ">::create(data);\n";
            }
            dbOut << "    }\n\n";
        }

        dbOut << "};\n\n";
    }

    // 2. Output Model Clients
    for (const auto& model : models) {
        dbOut << "struct " << model.name << "Client {\n";
        dbOut << "    drogon::Task<void> create(const Json::Value& data) const {\n";
        dbOut << "        co_await ::" << model.name << "::create(data);\n";
        dbOut << "    }\n\n";

        // Create overload if model has username & password
        bool hasUsername = false;
        bool hasPassword = false;
        for (const auto& f : model.fields) {
            if (f.name == "username") hasUsername = true;
            if (f.name == "password") hasPassword = true;
        }
        if (hasUsername && hasPassword) {
            dbOut << "    drogon::Task<void> create(const std::string& username, const std::string& password) const {\n";
            dbOut << "        co_await ::" << model.name << "::create(username, password);\n";
            dbOut << "    }\n\n";
        }

        dbOut << "    drogon::Task<Json::Value> findUnique(const Json::Value& query) const {\n";
        dbOut << "        Json::Value where = query;\n";
        dbOut << "        Json::Value include;\n";
        dbOut << "        if (query.isObject()) {\n";
        dbOut << "            if (query.isMember(\"where\")) {\n";
        dbOut << "                where = query[\"where\"];\n";
        dbOut << "            } else if (query.isMember(\"include\")) {\n";
        dbOut << "                where = query;\n";
        dbOut << "                where.removeMember(\"include\");\n";
        dbOut << "            }\n";
        dbOut << "            if (query.isMember(\"include\")) {\n";
        dbOut << "                include = query[\"include\"];\n";
        dbOut << "            }\n";
        dbOut << "        }\n";
        dbOut << "        Json::Value result = co_await ::" << model.name << "::findUnique(where);\n";
        dbOut << "        if (result.isNull() || include.isNull() || !include.isObject()) {\n";
        dbOut << "            co_return result;\n";
        dbOut << "        }\n";
        for (const auto& f : model.fields) {
            if (!f.isRelation) continue;
            dbOut << "        if (include.isMember(\"" << f.name << "\") && include[\"" << f.name << "\"].asBool()) {\n";
            dbOut << "            if (result.isMember(\"" << f.relationFields << "\") && !result[\"" << f.relationFields << "\"].isNull()) {\n";
            dbOut << "                Json::Value subWhere;\n";
            dbOut << "                subWhere[\"" << f.relationReferences << "\"] = result[\"" << f.relationFields << "\"];\n";
            if (f.isList) {
                dbOut << "                result[\"" << f.name << "\"] = co_await ::" << f.relationModel << "::findMany(subWhere);\n";
            } else {
                dbOut << "                result[\"" << f.name << "\"] = co_await ::" << f.relationModel << "::findUnique(subWhere);\n";
            }
            dbOut << "            } else {\n";
            if (f.isList) {
                dbOut << "                result[\"" << f.name << "\"] = Json::Value(Json::arrayValue);\n";
            } else {
                dbOut << "                result[\"" << f.name << "\"] = Json::Value(Json::nullValue);\n";
            }
            dbOut << "            }\n";
            dbOut << "        }\n";
        }
        dbOut << "        co_return result;\n";
        dbOut << "    }\n\n";

        dbOut << "    drogon::Task<Json::Value> findMany(const Json::Value& query = Json::Value()) const {\n";
        dbOut << "        Json::Value where = query;\n";
        dbOut << "        Json::Value include;\n";
        dbOut << "        if (query.isObject()) {\n";
        dbOut << "            if (query.isMember(\"where\")) {\n";
        dbOut << "                where = query[\"where\"];\n";
        dbOut << "            } else if (query.isMember(\"include\")) {\n";
        dbOut << "                where = query;\n";
        dbOut << "                where.removeMember(\"include\");\n";
        dbOut << "            }\n";
        dbOut << "            if (query.isMember(\"include\")) {\n";
        dbOut << "                include = query[\"include\"];\n";
        dbOut << "            }\n";
        dbOut << "        }\n";
        dbOut << "        Json::Value results = co_await ::" << model.name << "::findMany(where);\n";
        dbOut << "        if (results.isNull() || !results.isArray() || results.empty() || include.isNull() || !include.isObject()) {\n";
        dbOut << "            co_return results;\n";
        dbOut << "        }\n";
        dbOut << "        for (auto& result : results) {\n";
        for (const auto& f : model.fields) {
            if (!f.isRelation) continue;
            dbOut << "            if (include.isMember(\"" << f.name << "\") && include[\"" << f.name << "\"].asBool()) {\n";
            dbOut << "                if (result.isMember(\"" << f.relationFields << "\") && !result[\"" << f.relationFields << "\"].isNull()) {\n";
            dbOut << "                    Json::Value subWhere;\n";
            dbOut << "                    subWhere[\"" << f.relationReferences << "\"] = result[\"" << f.relationFields << "\"];\n";
            if (f.isList) {
                dbOut << "                    result[\"" << f.name << "\"] = co_await ::" << f.relationModel << "::findMany(subWhere);\n";
            } else {
                dbOut << "                    result[\"" << f.name << "\"] = co_await ::" << f.relationModel << "::findUnique(subWhere);\n";
            }
            dbOut << "                } else {\n";
            if (f.isList) {
                dbOut << "                    result[\"" << f.name << "\"] = Json::Value(Json::arrayValue);\n";
            } else {
                dbOut << "                    result[\"" << f.name << "\"] = Json::Value(Json::nullValue);\n";
            }
            dbOut << "                }\n";
            dbOut << "            }\n";
        }
        dbOut << "        }\n";
        dbOut << "        co_return results;\n";
        dbOut << "    }\n\n";

        dbOut << "    drogon::Task<Json::Value> findFirst(const Json::Value& query) const {\n";
        dbOut << "        co_return co_await findUnique(query);\n";
        dbOut << "    }\n\n";

        dbOut << "    drogon::Task<void> update(const Json::Value& query) const {\n";
        dbOut << "        Json::Value w = query.isMember(\"where\") ? query[\"where\"] : Json::Value(Json::objectValue);\n";
        dbOut << "        Json::Value d = query.isMember(\"data\") ? query[\"data\"] : Json::Value(Json::objectValue);\n";
        dbOut << "        co_await ::" << model.name << "::update(w, d);\n";
        dbOut << "    }\n\n";

        dbOut << "    drogon::Task<void> deleteMany(const Json::Value& query) const {\n";
        dbOut << "        Json::Value w = query.isMember(\"where\") ? query[\"where\"] : query;\n";
        dbOut << "        co_await ::" << model.name << "::deleteMany(w);\n";
        dbOut << "    }\n\n";

        // FindBy[FieldName] helpers
        for (const auto& f : model.fields) {
            if (f.isRelation) continue;
            if (f.isUnique || f.isPrimaryKey) {
                std::string capName = f.name;
                if (!capName.empty()) capName[0] = std::toupper(capName[0]);

                std::string paramType = "const std::string&";
                if (f.type == "Serial" || f.type == "Int") paramType = "int64_t";
                else if (f.type == "Float") paramType = "double";
                else if (f.type == "Boolean") paramType = "bool";

                dbOut << "    drogon::Task<Json::Value> findBy" << capName << "(" << paramType << " val) const {\n";
                dbOut << "        co_return co_await ::" << model.name << "::findBy" << capName << "(val);\n";
                dbOut << "    }\n\n";
            }
        }

        dbOut << "};\n\n";
    }

    // 3. Write the unified XpdClient struct
    dbOut << "struct XpdClient {\n";
    for (const auto& model : models) {
        std::string camelName = model.name;
        if (!camelName.empty()) camelName[0] = std::tolower(camelName[0]);
        dbOut << "    " << model.name << "Client " << camelName << ";\n";
    }
    dbOut << "};\n\n";

    // 3b. Write the TxClient struct and beginTransaction function
    dbOut << "struct TxClient {\n";
    dbOut << "    drogon::orm::TransactionPtr tx;\n\n";
    dbOut << "    drogon::Task<void> commit() { co_await tx->commitAsync(); }\n";
    dbOut << "    drogon::Task<void> rollback() { co_await tx->rollbackAsync(); }\n\n";
    for (const auto& model : models) {
        std::string camelName = model.name;
        if (!camelName.empty()) camelName[0] = std::tolower(camelName[0]);

        dbOut << "    struct " << model.name << "ClientTx {\n";
        dbOut << "        drogon::orm::TransactionPtr tx;\n";
        dbOut << "        drogon::Task<void> create(const Json::Value& data) const { co_await ::" << model.name << "::create(tx, data); }\n";
        dbOut << "        drogon::Task<xp::var> findUnique(const Json::Value& query) const { co_return co_await ::" << model.name << "::findUnique(tx, query); }\n";
        dbOut << "        drogon::Task<xp::var> findMany(const Json::Value& query = Json::Value()) const { co_return co_await ::" << model.name << "::findMany(tx, query); }\n";
        dbOut << "        drogon::Task<void> update(const Json::Value& where, const Json::Value& data) const { co_await ::" << model.name << "::update(tx, where, data); }\n";
        dbOut << "        drogon::Task<void> deleteMany(const Json::Value& where) const { co_await ::" << model.name << "::deleteMany(tx, where); }\n";

        for (const auto& f : model.fields) {
            if (f.isRelation) continue;
            if (f.isUnique || f.isPrimaryKey) {
                std::string capName = f.name;
                if (!capName.empty()) capName[0] = std::toupper(capName[0]);

                std::string paramType = "const std::string&";
                if (f.type == "Serial" || f.type == "Int") paramType = "int64_t";
                else if (f.type == "Float") paramType = "double";
                else if (f.type == "Boolean") paramType = "bool";

                dbOut << "        drogon::Task<Json::Value> findBy" << capName << "(" << paramType << " val) const {\n";
                dbOut << "            Json::Value where;\n";
                dbOut << "            where[\"" << f.name << "\"] = val;\n";
                dbOut << "            co_return co_await ::" << model.name << "::findUnique(tx, where);\n";
                dbOut << "        }\n";
            }
        }
        dbOut << "    } " << camelName << "{tx};\n\n";
    }
    dbOut << "};\n\n";

    dbOut << "inline drogon::Task<TxClient> beginTransaction() {\n";
    dbOut << "    auto tx = co_await xp::DatabaseManager::instance().newTransaction();\n";
    dbOut << "    co_return TxClient{tx};\n";
    dbOut << "}\n\n";

    dbOut << "inline constexpr XpdClient xpd{};\n\n";

    // 4. Write SchemaSync inside db.h
    dbOut << "class SchemaSync {\n";
    dbOut << "public:\n";
    dbOut << "    static drogon::Task<void> syncAll() {\n";
    if (dbProvider == "sqlite" || dbProvider == "sqlite3") {
        dbOut << "        // Enable FK enforcement for SQLite\n";
        dbOut << "        { auto _fk_client = drogon::app().getDbClient(\"default\");\n";
        dbOut << "          if (_fk_client) { try { co_await xp::executeParameterized(_fk_client, \"PRAGMA foreign_keys = ON;\", {}); } catch (...) {} } }\n";
    }
    dbOut << "        co_await xp::DatabaseManager::instance().runMigrations();\n";
    for (const auto& model : models) {
        dbOut << "        co_await ::" << model.name << "::sync();\n";
    }
    dbOut << "        co_return;\n";
    dbOut << "    }\n";
    dbOut << "};\n\n";
    dbOut << "struct AutoSyncRegister {\n";
    dbOut << "    AutoSyncRegister() {\n";
    dbOut << "        xp::DatabaseManager::instance().registerSync([]() -> drogon::Task<void> {\n";
    dbOut << "            co_await SchemaSync::syncAll();\n";
    dbOut << "        });\n";
    dbOut << "    }\n";
    dbOut << "};\n";
    dbOut << "inline AutoSyncRegister auto_sync_register_instance;\n";

    dbOut.close();
    success("Generated unified Xpress++ DB client in build/generated/db.h");
    divider();

    info("Rebuilding and running database migrations...");
    if (build(false) != 0) {
        error("Failed to build the project for migrations.");
        return 1;
    }

    const auto binary = projectBinary();
    if (binary.empty()) {
        error("No executable found in ./build for migrations.");
        return 1;
    }

    info("Running migrations: " + binary + " --migrate");
    divider();
    int rc = runCommand(binary + " --migrate");
    if (rc == 0) {
        success("Database migrations applied successfully!");
    } else {
        error("Failed to apply database migrations.");
    }
    return rc;
}

static std::string detectProjectName() {
    std::ifstream f("CMakeLists.txt");
    if (!f) return "app";
    std::string line;
    while (std::getline(f, line)) {
        auto pos = line.find("project(");
        if (pos != std::string::npos) {
            auto end = line.find(")", pos);
            if (end != std::string::npos) {
                std::string content = line.substr(pos + 8, end - pos - 8);
                std::stringstream ss(content);
                std::string name;
                if (ss >> name) {
                    return name;
                }
            }
        }
    }
    return "app";
}

int dockerize() {
    std::string reason;
    if (!isXpressProject(&reason)) {
        error("Not an Xpress++ project.", reason, "Run this command from your project root directory.");
        return 1;
    }

    std::string name = detectProjectName();
    info("Configuring Docker build for project: " + name);

    std::ofstream df("Dockerfile");
    if (!df) {
        error("Failed to create Dockerfile.", "Check write permissions in the current directory.");
        return 1;
    }

    df << "# Multi-stage Docker build for " << name << "\n"
       << "# -----------------------------------------------------\n"
       << "# Stage 1: Build stage\n"
       << "FROM ubuntu:22.04 AS builder\n\n"
       << "ENV DEBIAN_FRONTEND=noninteractive\n"
       << "RUN apt-get update && apt-get install -y \\\n"
       << "    build-essential \\\n"
       << "    cmake \\\n"
       << "    git \\\n"
       << "    libssl-dev \\\n"
       << "    libjsoncpp-dev \\\n"
       << "    uuid-dev \\\n"
       << "    zlib1g-dev \\\n"
       << "    libsqlite3-dev \\\n"
       << "    libpq-dev \\\n"
       << "    && rm -rf /var/lib/apt/lists/*\n\n"
       << "WORKDIR /app\n"
       << "COPY . .\n\n"
       << "RUN mkdir -p build && cd build \\\n"
       << "    && cmake -DCMAKE_BUILD_TYPE=Release .. \\\n"
       << "    && make -j$(nproc)\n\n"
       << "# -----------------------------------------------------\n"
       << "# Stage 2: Runtime stage\n"
       << "FROM ubuntu:22.04\n\n"
       << "ENV DEBIAN_FRONTEND=noninteractive\n"
       << "RUN apt-get update && apt-get install -y \\\n"
       << "    libssl3 \\\n"
       << "    libjsoncpp25 \\\n"
       << "    uuid-dev \\\n"
       << "    zlib1g \\\n"
       << "    libsqlite3-0 \\\n"
       << "    libpq5 \\\n"
       << "    && rm -rf /var/lib/apt/lists/*\n\n"
       << "WORKDIR /app\n\n"
       << "# Copy binary and default files\n"
       << "COPY --from=builder /app/build/" << name << " .\n"
       << "COPY --from=builder /app/config.json ./config.json 2>/dev/null || true\n"
       << "COPY --from=builder /app/.env.example ./.env.example 2>/dev/null || true\n"
       << "COPY --from=builder /app/uploads ./uploads 2>/dev/null || true\n\n"
       << "EXPOSE 8080\n\n"
       << "CMD [\"./" << name << "\"]\n";

    df.close();

    std::ofstream di(".dockerignore");
    if (di) {
        di << "build/\n"
           << ".vscode/\n"
           << "uploads/tmp/\n"
           << ".env\n"
           << "*.db\n"
           << "*.log\n"
           << ".git/\n"
           << "Dockerfile\n"
           << ".dockerignore\n";
        di.close();
    }

    divider();
    success("Created Dockerfile and .dockerignore for project " + name);
    std::cout << "\n"
              << "  You can build and run this container using:\n"
              << "    " << colour::cyan() << "docker build -t " << name << " ." << colour::reset() << "\n"
              << "    " << colour::cyan() << "docker run -p 8080:8080 --env-file .env " << name << colour::reset() << "\n\n";

    return 0;
}

} // namespace xp::cli
