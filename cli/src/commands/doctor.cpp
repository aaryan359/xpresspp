#include "commands/commands.h"
#include "util/util.h"

#include <iostream>
#include <sstream>
#include <vector>


namespace xp::cli {

// ── Checks ───────────────────────────────────────────────────────────────────

static std::string firstLine(const std::string& text) {
    std::istringstream stream(text);
    std::string line;
    std::getline(stream, line);
    return line;
}

static bool drogonInstalled() {
    return !captureCommand("find /usr /opt/homebrew -name 'DrogonConfig.cmake' 2>/dev/null | head -1").empty();
}

static bool jsoncppInstalled() {
    return !captureCommand("find /usr /opt/homebrew -path '*jsoncpp/json/json.h' 2>/dev/null | head -1").empty();
}

struct Check {
    std::string name;
    bool        found;
    std::string detail;  // shown when found (e.g. a version)
    std::string fix;     // shown when missing
};

int doctor() {
    const std::vector<Check> checks = {
        {"cmake",     commandExists("cmake"), firstLine(captureCommand("cmake --version")),
                      "sudo apt install cmake"},
        {"C++ compiler", commandExists("c++"), firstLine(captureCommand("c++ --version")),
                      "sudo apt install g++"},
        {"jsoncpp",   jsoncppInstalled(), "",
                      "sudo apt install libjsoncpp-dev"},
        {"Drogon",    drogonInstalled(), "",
                      "xp doctor --fix"},
        {"Xpress++ headers", fs::exists(frameworkRoot() / "core/include/xpresspp/xpresspp.h"),
                      frameworkRoot().string(),
                      "export XPRESSPP_HOME=/path/to/xpress++"},
    };

    std::cout << "\n" << colour::cyan() << colour::bold() << "  Xpress++ doctor" << colour::reset() << "\n";
    divider();

    int missing = 0;
    for (const auto& check : checks) {
        if (check.found) {
            success(check.name + "  " + colour::dim() + check.detail + colour::reset());
        } else {
            warn(check.name + " is missing.  Fix: " + check.fix);
            ++missing;
        }
    }

    divider();
    if (missing == 0) success("Everything is ready.");
    else              std::cout << "  " << missing << " problem(s). Fix them, then run `xp doctor` again.\n";
    std::cout << "\n";
    return missing == 0 ? 0 : 1;
}

// ── Installing dependencies ──────────────────────────────────────────────────

struct PackageManager {
    const char* program;
    const char* installCommand;
};

// Add a line here to support another Linux distribution.
static const std::vector<PackageManager> packageManagers = {
    {"apt-get", "sudo apt-get update -qq && sudo apt-get install -y build-essential cmake git "
                "libssl-dev libjsoncpp-dev zlib1g-dev uuid-dev"},
    {"dnf",     "sudo dnf install -y gcc-c++ cmake git openssl-devel jsoncpp-devel zlib-devel libuuid-devel"},
    {"pacman",  "sudo pacman -Sy --noconfirm base-devel cmake git jsoncpp openssl zlib util-linux-libs"},
    {"brew",    "brew install cmake jsoncpp openssl zlib ossp-uuid"},
};

static int buildDrogonFromSource() {
    const std::string dir = "/tmp/xpresspp_drogon";
    const std::string flags = " -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF -DBUILD_CTL=OFF -DBUILD_ORM=OFF";

    if (runCommand("rm -rf " + dir + " && git clone --depth 1 --recurse-submodules "
                   "https://github.com/drogonframework/drogon " + dir) != 0) return 1;
    if (runCommand("cmake -S " + dir + " -B " + dir + "/build" + flags) != 0) return 1;
    if (runCommand("cmake --build " + dir + "/build --parallel") != 0) return 1;
    return runCommand("sudo cmake --install " + dir + "/build");
}



int installDeps() {
    std::cout << "\n" << colour::cyan() << colour::bold() << "  Installing dependencies" << colour::reset() << "\n";
    divider();

    const PackageManager* manager = nullptr;
    for (const auto& candidate : packageManagers) {
        if (commandExists(candidate.program)) { manager = &candidate; break; }
    }
    if (!manager) {
        error("No supported package manager found.", "Tried: apt-get, dnf, pacman, brew",
              "Install cmake, a C++ compiler, jsoncpp and Drogon by hand.");
        return 1;
    }

    info(std::string("Using ") + manager->program);
    if (runCommand(manager->installCommand) != 0) {
        error("Installing system packages failed.");
        return 1;
    }

    if (drogonInstalled()) {
        success("Drogon is already installed.");
    } else {
        info("Building Drogon from source (a few minutes)...");
        if (buildDrogonFromSource() != 0) {
            error("Building Drogon failed.", "The output above shows why.");
            return 1;
        }
    }

    divider();
    success("Done. Run `xp doctor` to check.");
    return 0;
}

} // namespace xp::cli
