#include "commands/commands.h"
#include "util/util.h"

#include <chrono>
#include <csignal>
#include <map>
#include <string>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace xp::cli {

using Snapshot = std::map<std::string, fs::file_time_type>;

static bool isSourceFile(const fs::path& path) {
    const std::string ext = path.extension().string();
    return ext == ".cpp" || ext == ".h" || ext == ".hpp" || path.filename() == "CMakeLists.txt";
}

// Remembers when every source file was last changed, so we can spot edits.
static Snapshot takeSnapshot() {
    Snapshot snapshot;
    std::error_code ignored;

    for (fs::recursive_directory_iterator it(".", ignored), end; it != end; it.increment(ignored)) {
        if (it->is_directory() && it->path().filename() == "build") {
            it.disable_recursion_pending();
            continue;
        }
        if (it->is_regular_file() && isSourceFile(it->path())) {
            snapshot[it->path().string()] = fs::last_write_time(it->path(), ignored);
        }
    }
    return snapshot;
}

// The running server, started as a child process so we can restart it.
class Server {
public:
    ~Server() { stop(); }

    void start() {
        const std::string binary = projectBinary();
        if (binary.empty()) {
            warn("No program found in ./build.");
            return;
        }
        pid_ = fork();
        if (pid_ == 0) {
            execl(binary.c_str(), binary.c_str(), nullptr);
            _exit(127);  // only reached if the program could not start
        }
        info("Server started (pid " + std::to_string(pid_) + ")");
    }

    void stop() {
        if (pid_ <= 0) return;
        kill(pid_, SIGTERM);
        waitpid(pid_, nullptr, 0);
        pid_ = -1;
    }

    // Notices if the server crashed on its own.
    void checkAlive() {
        if (pid_ > 0 && waitpid(pid_, nullptr, WNOHANG) == pid_) {
            warn("The server stopped. Edit a file to restart it.");
            pid_ = -1;
        }
    }

private:
    pid_t pid_ = -1;
};

int watch() {
    if (!requireProject()) return 1;

    Server server;
    if (build(false) == 0) server.start();

    Snapshot last = takeSnapshot();
    info("Watching for changes. Press Ctrl+C to stop.");
    divider();

    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        server.checkAlive();

        Snapshot current = takeSnapshot();
        if (current == last) continue;
        last = std::move(current);

        info("Change detected — rebuilding...");
        server.stop();
        if (build(false) == 0) server.start();
        else warn("Fix the errors above and save again.");
    }
}

} // namespace xp::cli
