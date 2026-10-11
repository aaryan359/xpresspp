#include "util/util.h"

#include <iostream>
#include <unistd.h>

namespace xp::cli {

namespace colour {

static const char* code(const char* ansi) {
    static const bool terminal = isatty(fileno(stdout)) != 0;
    return terminal ? ansi : "";
}

const char* red()    { return code("\033[31m"); }
const char* green()  { return code("\033[32m"); }
const char* yellow() { return code("\033[33m"); }
const char* cyan()   { return code("\033[36m"); }
const char* bold()   { return code("\033[1m");  }
const char* dim()    { return code("\033[2m");  }
const char* reset()  { return code("\033[0m");  }

} // namespace colour

void info(const std::string& message) {
    std::cout << colour::cyan() << "  ▸ " << colour::reset() << message << "\n";
}

void success(const std::string& message) {
    std::cout << colour::green() << colour::bold() << "  ✓ " << colour::reset() << message << "\n";
}

void warn(const std::string& message) {
    std::cout << colour::yellow() << "  ⚠ " << message << colour::reset() << "\n";
}

void error(const std::string& heading, const std::string& detail, const std::string& fix) {
    std::cerr << "\n" << colour::red() << colour::bold() << "  ✗ " << heading << colour::reset() << "\n";
    if (!detail.empty()) std::cerr << colour::dim() << "    " << detail << colour::reset() << "\n";
    if (!fix.empty())    std::cerr << "\n" << colour::yellow() << "  → Fix: " << fix << colour::reset() << "\n";
    std::cerr << "\n";
}

void divider() {
    std::cout << colour::dim() << "  ─────────────────────────────────────────" << colour::reset() << "\n";
}

void usage() {
    std::cout << "\n"
        << colour::cyan() << colour::bold() << "  Xpress++  —  Express-style C++ servers\n" << colour::reset()
        << "\n  Usage:\n" << colour::dim()
        << "    xp create <name>         Create a new project\n"
        << "    xp dev                   Build, run, and restart when files change\n"
        << "    xp build [--release]     Compile the project\n"
        << "    xp run   [--release]     Build and run the server\n"
        << "    xp generate <type> <n>   Make a route, controller, middleware, or model (alias: g)\n"
        << "    xp clean                 Delete the build folder\n"
        << "    xp doctor [--fix]        Check (or install) dependencies\n"
        << "    xp version               Print the version\n"
        << colour::reset() << "\n";
}

} // namespace xp::cli
