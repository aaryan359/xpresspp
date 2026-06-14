#include "cli/commands.h"
#include "cli/diagnostics.h"
#include "cli/common.h"
#include <iostream>
#include <fstream>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <sstream>

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
    gitignore
        << "build/\n"
        << ".vscode/\n"
        << "uploads/tmp/\n"
        << ".env\n"          // never commit secrets
        << "*.db\n"
        << "*.log\n";

    divider();
    success(std::string("Created project  ") + colour::bold() + name + colour::reset());
    std::cout
        << "\n"
        << colour::bold() << "  Next steps:\n" << colour::reset()
        << colour::cyan()
        << "    cd " << name << "\n"
        << "    cp .env.example .env      # set your config\n"
        << "    xp watch                  # build, run, and live-reload\n"
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
//  Migrate Command (Prisma-like Schema Parser and Generator)
// ============================================================
struct FieldInfo {
    std::string name;
    std::string type;
    bool nullable = false;
    bool isPrimaryKey = false;
    bool isUnique = false;
    bool isDefaultNow = false;
    bool isRelation = false;
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

int migrate() {
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

    std::vector<ModelInfo> models;
    ModelInfo currentModel;
    bool inModel = false;
    std::string line;

    while (std::getline(file, line)) {
        line = trimString(line);
        if (line.empty() || line.rfind("//", 0) == 0) {
            continue;
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

    const fs::path modelsDir = fs::current_path() / "src" / "models";
    fs::create_directories(modelsDir);

    info("Generating unified C++ DB client in src/models/db.h ...");

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
        dbOut << "class " << model.name << " : public xp::Model<" << model.name << "> {\n";
        dbOut << "public:\n";
        dbOut << "    using xp::Model<" << model.name << ">::create;\n\n";
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
            dbOut << "        co_await xp::Model<" << model.name << ">::create(data);\n";
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

        dbOut << "    drogon::Task<Json::Value> findUnique(const Json::Value& where) const {\n";
        dbOut << "        co_return co_await ::" << model.name << "::findUnique(where);\n";
        dbOut << "    }\n\n";

        dbOut << "    drogon::Task<Json::Value> findMany(const Json::Value& where = Json::Value()) const {\n";
        dbOut << "        co_return co_await ::" << model.name << "::findMany(where);\n";
        dbOut << "    }\n\n";

        dbOut << "    drogon::Task<Json::Value> findFirst(const Json::Value& where) const {\n";
        dbOut << "        co_return co_await ::" << model.name << "::findUnique(where);\n";
        dbOut << "    }\n\n";

        dbOut << "    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) const {\n";
        dbOut << "        co_await ::" << model.name << "::update(where, data);\n";
        dbOut << "    }\n\n";

        dbOut << "    drogon::Task<void> deleteMany(const Json::Value& where) const {\n";
        dbOut << "        co_await ::" << model.name << "::deleteMany(where);\n";
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

    // 3. Write the unified PrismaClient struct
    dbOut << "struct PrismaClient {\n";
    for (const auto& model : models) {
        std::string camelName = model.name;
        if (!camelName.empty()) camelName[0] = std::tolower(camelName[0]);
        dbOut << "    " << model.name << "Client " << camelName << ";\n";
    }
    dbOut << "};\n\n";
    dbOut << "inline constexpr PrismaClient prisma{};\n\n";

    // 4. Write SchemaSync inside db.h
    dbOut << "class SchemaSync {\n";
    dbOut << "public:\n";
    dbOut << "    static drogon::Task<void> syncAll() {\n";
    for (const auto& model : models) {
        dbOut << "        co_await ::" << model.name << "::sync();\n";
    }
    dbOut << "        co_return;\n";
    dbOut << "    }\n";
    dbOut << "};\n";

    success("Generated unified Prisma-like client in src/models/db.h");
    divider();
    success("Migration structures generated successfully! You can now use 'prisma' in your controllers and SchemaSync::syncAll() in your DB configuration.");
    return 0;
}

} // namespace xp::cli
