#pragma once

// Small helpers shared by every command.
// Each section says which .cpp file implements it.

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace xp::cli {

// ── console.cpp ──────────────────────────────────────────────────────────────
// Colours are empty strings when output is not a terminal (pipes, CI).
namespace colour {
    const char* red();
    const char* green();
    const char* yellow();
    const char* cyan();
    const char* bold();
    const char* dim();
    const char* reset();
}

void info(const std::string& message);
void success(const std::string& message);
void warn(const std::string& message);
void error(const std::string& heading, const std::string& detail = "", const std::string& fix = "");
void divider();
void usage();

// ── process.cpp ──────────────────────────────────────────────────────────────
int         runCommand(const std::string& command, bool echo = true);  // returns exit code
std::string captureCommand(const std::string& command);                // returns stdout
bool        commandExists(const std::string& name);                    // is it on PATH?

// ── paths.cpp ────────────────────────────────────────────────────────────────
fs::path frameworkRoot();        // folder that holds core/ and examples/
bool     requireProject();       // prints an error and returns false if cwd is not a project
std::string projectBinary();     // the executable built into ./build ("" if none)

// ── diagnostics.cpp ──────────────────────────────────────────────────────────
// Runs a build command, prints coloured progress, and returns its exit code.
// The full compiler output is stored in `output` so errors can be explained.
int  runBuild(const std::string& command, std::string& output);
void explainErrors(const std::string& compilerOutput);

} // namespace xp::cli
