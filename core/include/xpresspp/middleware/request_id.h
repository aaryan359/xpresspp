#pragma once

#include "../router.h"

#include <atomic>
#include <memory>
#include <sstream>
#include <string>

namespace xp {

inline Middleware requestId(const std::string& header = "X-Request-Id") {
    auto counter = std::make_shared<std::atomic<unsigned long long>>(0);
    return [header, counter](Request& req, Response& res, Next next) {
        auto id = req.header(header);
        if (id.empty()) {
            std::ostringstream generated;
            generated << "req-" << counter->fetch_add(1, std::memory_order_relaxed);
            id = generated.str();
        }
        res.header(header, id);
        next();
    };
}

} // namespace xp
