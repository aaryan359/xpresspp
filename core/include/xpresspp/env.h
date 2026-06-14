#pragma once

// ============================================================
//  xpress++ env.h
//  One-call .env file loader.
//  Usage:  xp::loadEnv();           // reads .env
//          xp::loadEnv(".env.prod"); // reads a specific file
// ============================================================

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>

namespace xp {

// Load KEY=VALUE pairs from a .env file into the process environment.
// • Skips blank lines and lines starting with #
// • Strips surrounding quotes from values ("value" or 'value')
// • Does NOT override variables that are already set in the environment
//   (so real env vars always win over the .env file)
// Returns the number of variables loaded.
inline int loadEnv(const std::string& filepath = ".env") {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        return 0;  // silently skip — .env is optional
    }

    int count = 0;
    std::string line;

    while (std::getline(file, line)) {
        // Strip carriage return (Windows line endings)
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        // Skip blanks and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }

        // Find the = separator
        const auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }

        std::string key   = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        // Trim whitespace from key
        auto trim = [](std::string& s) {
            const auto start = s.find_first_not_of(" \t");
            if (start == std::string::npos) { s.clear(); return; }
            const auto end = s.find_last_not_of(" \t");
            s = s.substr(start, end - start + 1);
        };
        trim(key);
        trim(value);

        if (key.empty()) {
            continue;
        }

        // Strip surrounding quotes from value
        if (value.size() >= 2 &&
            ((value.front() == '"'  && value.back() == '"') ||
             (value.front() == '\'' && value.back() == '\''))) {
            value = value.substr(1, value.size() - 2);
        }

        // Don't override real environment variables
        if (std::getenv(key.c_str()) != nullptr) {
            continue;
        }

        // setenv (POSIX)
#if defined(_WIN32)
        _putenv_s(key.c_str(), value.c_str());
#else
        ::setenv(key.c_str(), value.c_str(), 1);
#endif
        ++count;
    }

    return count;
}

} // namespace xp
