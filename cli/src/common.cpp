#include "cli/common.h"
#include <iostream>
#include <cstdlib>
#include <array>
#include <sstream>

#if !defined(_WIN32)
#include <unistd.h>
#endif

namespace xp::cli {

namespace colour {

bool ok() {
#if defined(_WIN32)
    return false;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

const char* red()    { return ok() ? "\033[31m" : ""; }
const char* green()  { return ok() ? "\033[32m" : ""; }
const char* yellow() { return ok() ? "\033[33m" : ""; }
const char* cyan()   { return ok() ? "\033[36m" : ""; }
const char* bold()   { return ok() ? "\033[1m"  : ""; }
const char* dim()    { return ok() ? "\033[2m"  : ""; }
const char* reset()  { return ok() ? "\033[0m"  : ""; }

} // namespace colour

void info(const std::string& msg) {
    std::cout << colour::cyan() << "  ▸ " << colour::reset() << msg << "\n";
}

void success(const std::string& msg) {
    std::cout << colour::green() << colour::bold()
              << "  ✓ " << colour::reset() << msg << "\n";
}

void warn(const std::string& msg) {
    std::cout << colour::yellow() << colour::bold()
              << "  ⚠ " << colour::reset() << colour::yellow() << msg
              << colour::reset() << "\n";
}

void error(const std::string& heading,
           const std::string& detail,
           const std::string& fix) {
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

void divider() {
    std::cout << colour::dim()
              << "  ─────────────────────────────────────────\n"
              << colour::reset();
}

void usage() {
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
        << "    xp migrate              Generate C++ models and synchronize database schema\n"
        << "    xp watch                Auto-rebuild on source changes\n"
        << "    xp clean                Delete the build directory\n"
        << "    xp install              Auto-install all dependencies (Drogon etc.)\n"
        << "    xp doctor               Check system dependencies\n"
        << "    xp doctor --fix         Same as xp install\n"
        << "    xp routes               List all registered routes (from build)\n"
        << colour::reset()
        << "\n";
}

int runCommand(const std::string& command, bool show) {
    if (show) {
        std::cout << colour::dim() << "  $ " << command
                  << colour::reset() << "\n";
    }
    return std::system(command.c_str());  // NOLINT
}

std::string captureCommand(const std::string& command) {
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

fs::path repoRoot() {
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

bool isXpressProject(std::string* reason) {
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

} // namespace xp::cli
