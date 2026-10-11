#include "commands/commands.h"
#include "util/util.h"

#include <iostream>
#include <string>
#include <vector>

using namespace xp::cli;

static bool hasFlag(const std::vector<std::string>& args, const std::string& flag) {
    for (const auto& arg : args) if (arg == flag) return true;
    return false;
}

int main(int argc, char** argv) {
    
    if (argc < 2) {
        usage();
        return 0;
    }

    const std::string command = argv[1];
    const std::vector<std::string> args(argv + 2, argv + argc);  // everything after the command

    if (command == "create") {
        if (args.empty()) { error("Missing project name.", "", "xp create my-app"); return 1; }
        return createApp(args[0]);
    }
    if (command == "generate" || command == "g") {
        if (args.size() < 2) { error("Missing type or name.", "", "xp generate route users"); return 1; }
        return generate(args[0], args[1]);
    }
    if (command == "build")   return build(hasFlag(args, "--release"));
    if (command == "run")     return run(hasFlag(args, "--release"));
    if (command == "dev" || command == "watch") return watch();
    if (command == "clean")   return clean();
    if (command == "install") return installDeps();
    if (command == "doctor")  return hasFlag(args, "--fix") ? installDeps() : doctor();
    if (command == "version" || command == "--version" || command == "-v") {
        std::cout << "Xpress++ 0.1.0-alpha.0\n";
        return 0;
    }

    error("Unknown command: \"" + command + "\"", "", "Run `xp` to see all commands.");
    return 1;
}
