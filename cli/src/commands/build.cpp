#include "commands/commands.h"
#include "util/util.h"

#include <fstream>

namespace xp::cli {

// If ./build was configured for a different folder (e.g. the project was
// copied), CMake refuses to reuse it. Delete it so it is configured fresh.
static void removeStaleBuildFolder() {
    std::ifstream cache("build/CMakeCache.txt");
    const std::string key = "CMAKE_HOME_DIRECTORY:INTERNAL=";
    std::string line;

    while (std::getline(cache, line)) {
        if (line.rfind(key, 0) != 0) continue;

        std::error_code ignored;
        
        if (!fs::equivalent(line.substr(key.size()), fs::current_path(), ignored)) {
            warn("./build belongs to another folder — deleting it.");
            fs::remove_all("build");
        }
        return;
    }
}

int build(bool release) {
    if (!requireProject()) return 1;
    removeStaleBuildFolder();

    const std::string type = release ? "Release" : "Debug";

    info("Configuring (" + type + ")...");
    if (runCommand("cmake -S . -B build -DCMAKE_BUILD_TYPE=" + type + " 2>&1") != 0) {
        error("CMake could not configure the project.",
              "The output above shows why.",
              "Run `xp doctor` to check that CMake, Drogon and jsoncpp are installed.");
        return 1;
    }

    info("Compiling...");
    std::string output;
    if (runBuild("cmake --build build --parallel 2>&1", output) != 0) {
        error("The build failed.");
        explainErrors(output);
        return 1;
    }

    success("Build finished.");
    return 0;
}

int run(bool release) {
    if (build(release) != 0) return 1;

    const std::string binary = projectBinary();
    if (binary.empty()) {
        error("The build finished but no program was found in ./build.");
        return 1;
    }

    info("Starting " + binary);
    divider();
    return runCommand(binary, false);
}

} // namespace xp::cli
