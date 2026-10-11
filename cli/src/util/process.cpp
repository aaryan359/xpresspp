#include "util/util.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace xp::cli {

int runCommand(const std::string& command, bool echo) {
    if (echo) std::cout << colour::dim() << "  $ " << command << colour::reset() << "\n";
    return std::system(command.c_str());
}

std::string captureCommand(const std::string& command) {
    std::string output;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return output;

    std::array<char, 256> buffer;
    while (fgets(buffer.data(), buffer.size(), pipe)) output += buffer.data();
    pclose(pipe);
    return output;
}

bool commandExists(const std::string& name) {
    return std::system(("command -v " + name + " >/dev/null 2>&1").c_str()) == 0;
}

} // namespace xp::cli
