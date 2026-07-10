#pragma once

#include "../router.h"

#include <chrono>
#include <iomanip>
#include <sstream>
#include <string>

namespace xp {

inline Middleware responseTime(std::string header = "X-Response-Time") {
    return [header = std::move(header)](Request&, Response& res, Next next) {
        const auto started = std::chrono::steady_clock::now();
        next();
        const auto elapsed = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started).count();
        std::ostringstream value;
        value << std::fixed << std::setprecision(2) << elapsed << "ms";
        res.header(header, value.str());
    };
}

} // namespace xp
