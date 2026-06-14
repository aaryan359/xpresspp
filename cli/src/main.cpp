#include "cli/common.h"
#include "cli/commands.h"
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        xp::cli::usage();
        return 0;
    }

    const std::string command = argv[1];
    const bool release =
        argc > 2 && std::string(argv[2]) == "--release";

    if (command == "create") {
        if (argc < 3) {
            xp::cli::error("Missing app name.",
                           "",
                           "xp create <app-name>");
            return 1;
        }
        return xp::cli::createApp(argv[2]);
    }

    if (command == "build")   return xp::cli::build(release);
    if (command == "run")     return xp::cli::run(release);
    if (command == "migrate") return xp::cli::migrate();
    if (command == "watch")   return xp::cli::watch();
    if (command == "clean")   return xp::cli::clean();
    if (command == "install") return xp::cli::installDeps();
    if (command == "doctor") {
        // Support: xp doctor --fix
        const bool fix = (argc > 2 && std::string(argv[2]) == "--fix");
        if (fix) return xp::cli::installDeps();
        return xp::cli::doctor();
    }

    xp::cli::error("Unknown command: \"" + command + "\"",
                  "",
                  "Run 'xp' with no arguments to see available commands.");
    return 1;
}
