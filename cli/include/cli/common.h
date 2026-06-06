#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace xp::cli {

// ANSI colour helpers
namespace colour {
    bool ok();
    const char* red();
    const char* green();
    const char* yellow();
    const char* cyan();
    const char* bold();
    const char* dim();
    const char* reset();
} // namespace colour

// Printing helpers
void info(const std::string& msg);
void success(const std::string& msg);
void warn(const std::string& msg);
void error(const std::string& heading, const std::string& detail = "", const std::string& fix = "");
void divider();
void usage();

// Command runner helpers
int runCommand(const std::string& command, bool show = true);
std::string captureCommand(const std::string& command);

// Environment helpers
fs::path repoRoot();
bool isXpressProject(std::string* reason = nullptr);

} // namespace xp::cli
