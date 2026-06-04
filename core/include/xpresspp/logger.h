#pragma once

#include "router.h"

#include <chrono>
#include <iostream>

namespace xp {

inline Middleware logger() {
    return [](Request& req, Response& res, Next next) {
        const auto started = std::chrono::steady_clock::now();
        next();
        const auto finished = std::chrono::steady_clock::now();
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(finished - started).count();

        std::cout << req.method() << " " << req.path() << " " << elapsed << "ms" << std::endl;
    };
}

} // namespace xp
