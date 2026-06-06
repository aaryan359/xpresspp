#pragma once

#include "router.h"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

namespace xp {

// ────────────────────────────────────────────────────────────
//  Internal: ANSI colour helpers (disabled on non-TTY)
// ────────────────────────────────────────────────────────────
namespace detail {

inline bool isColorSupported() {
#if defined(_WIN32)
    return false;
#else
    // Only colorise when stdout is a real terminal
    return isatty(fileno(stdout)) != 0;
#endif
}

inline const char* methodColor(const std::string& method) {
    if (!isColorSupported()) return "";
    if (method == "GET")     return "\033[32m";  // green
    if (method == "POST")    return "\033[34m";  // blue
    if (method == "PUT")     return "\033[33m";  // yellow
    if (method == "PATCH")   return "\033[35m";  // magenta
    if (method == "DELETE")  return "\033[31m";  // red
    if (method == "OPTIONS") return "\033[36m";  // cyan
    if (method == "HEAD")    return "\033[90m";  // grey
    return "\033[37m";                           // white
}

inline const char* statusColor(int code) {
    if (!isColorSupported()) return "";
    if (code >= 500) return "\033[31m";          // red
    if (code >= 400) return "\033[33m";          // yellow
    if (code >= 300) return "\033[36m";          // cyan
    if (code >= 200) return "\033[32m";          // green
    return "\033[37m";
}

inline const char* reset() {
    return isColorSupported() ? "\033[0m" : "";
}

inline const char* dim() {
    return isColorSupported() ? "\033[2m" : "";
}

inline const char* bold() {
    return isColorSupported() ? "\033[1m" : "";
}

// Pad a string to a fixed width (right-aligned with spaces on the left)
inline std::string padLeft(const std::string& s, std::size_t width) {
    if (s.size() >= width) return s;
    return std::string(width - s.size(), ' ') + s;
}

} // namespace detail

// ────────────────────────────────────────────────────────────
//  logger() — drop-in request-logging middleware
//
//  Output format (coloured in a terminal):
//    POST  /api/users  201  3ms  214 B
// ────────────────────────────────────────────────────────────
inline Middleware logger() {
    return [](Request& req, Response& res, Next next) {
        const auto started = std::chrono::steady_clock::now();
        next();
        const auto finished  = std::chrono::steady_clock::now();
        const auto elapsed   = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   finished - started).count();

        const int  code      = res.statusCode();
        const auto method    = req.method();
        const auto path      = req.path();

        // Build elapsed time string
        std::string time_str;
        if (elapsed < 1000) {
            time_str = std::to_string(elapsed) + "ms";
        } else {
            time_str = std::to_string(elapsed / 1000) + "." +
                       std::to_string((elapsed % 1000) / 10) + "s";
        }

        // ──────────────────────────────────────────────────
        //  Print the log line
        // ──────────────────────────────────────────────────
        std::cout
            << detail::methodColor(method) << detail::bold()
            << detail::padLeft(method, 7)          // right-pad method
            << detail::reset()
            << "  "
            << path
            << "  "
            << detail::statusColor(code) << detail::bold()
            << code
            << detail::reset()
            << detail::dim()
            << "  " << time_str
            << detail::reset()
            << "\n";
    };
}

// ────────────────────────────────────────────────────────────
//  combinedLogger() — Apache/Nginx combined-log format
//  Useful for log aggregation (Loki, Datadog, etc.)
//
//  Format:
//    [2026-01-02 15:04:05] GET /api/ping 200 3ms - "Mozilla/5.0 ..."
// ────────────────────────────────────────────────────────────
inline Middleware combinedLogger() {
    return [](Request& req, Response& res, Next next) {
        const auto started = std::chrono::steady_clock::now();
        next();
        const auto finished = std::chrono::steady_clock::now();
        const auto elapsed  = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  finished - started).count();

        // Timestamp
        const auto now_t = std::chrono::system_clock::to_time_t(
                               std::chrono::system_clock::now());
        char ts[24];
        std::strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", std::localtime(&now_t));

        const int  code   = res.statusCode();
        const auto method = req.method();
        const auto path   = req.path();
        const auto ip     = req.ip();
        const auto ua     = req.userAgent();

        std::cout
            << "[" << ts << "] "
            << detail::methodColor(method)  << method << detail::reset()
            << " " << path
            << " " << detail::statusColor(code) << code << detail::reset()
            << " " << elapsed << "ms"
            << " - \"" << ua << "\""
            << "\n";
    };
}

} // namespace xp
