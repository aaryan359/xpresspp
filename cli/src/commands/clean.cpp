#include "commands/commands.h"
#include "util/util.h"

namespace xp::cli {

int clean() {
    if (!fs::exists("build")) {
        info("Nothing to clean.");
        return 0;
    }

    std::error_code failure;
    fs::remove_all("build", failure);
    if (failure) {
        error("Could not delete ./build", failure.message());
        return 1;
    }
    success("Deleted ./build");
    return 0;
}

} // namespace xp::cli
