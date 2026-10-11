#include "util/util.h"

#include <climits>
#include <cstdlib>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace xp::cli {

static bool isFrameworkRoot(const fs::path& dir) {
    return fs::exists(dir / "core" / "include" / "xpresspp" / "xpresspp.h");
}

static fs::path executablePath() {
    char buffer[PATH_MAX];
#if defined(__APPLE__)
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) == 0) return buffer;
#else
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer));
    if (length > 0) return std::string(buffer, length);
#endif
    return {};
}

// Looks for the framework in this order:
//   1. $XPRESSPP_HOME             (set by the installer)
//   2. a parent folder of the xp binary (running from a source checkout)
//   3. ~/.xpresspp
fs::path frameworkRoot() {
    if (const char* home = std::getenv("XPRESSPP_HOME")) return home;

    for (fs::path dir = executablePath(); dir.has_parent_path() && dir != dir.parent_path();) {
        dir = dir.parent_path();
        if (isFrameworkRoot(dir)) return dir;
    }

    if (const char* home = std::getenv("HOME")) {
        const fs::path installed = fs::path(home) / ".xpresspp";
        if (isFrameworkRoot(installed)) return installed;
    }
    return fs::current_path();
}

bool requireProject() {
    if (fs::exists("CMakeLists.txt") && fs::exists("main.cpp")) return true;
    error("This folder is not an Xpress++ project.",
          "Expected CMakeLists.txt and main.cpp in " + fs::current_path().string(),
          "cd into your project folder, or create one with: xp create my-app");
    return false;
}

std::string projectBinary() {
    if (!fs::exists("build")) return "";

    for (const auto& entry : fs::directory_iterator("build")) {
        const bool isExecutable = entry.is_regular_file() &&
            (entry.status().permissions() & fs::perms::owner_exec) != fs::perms::none;
        if (isExecutable) return entry.path().string();
    }
    return "";
}

} // namespace xp::cli
