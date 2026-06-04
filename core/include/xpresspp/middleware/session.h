#pragma once

#include "../router.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace xp {

struct SessionOptions {
    std::string cookie = "xpresspp_session";
    int maxAgeSeconds = 86400;
    bool secure = false;
};

inline Middleware session(SessionOptions options = {}) {
    struct SessionState {
        std::chrono::steady_clock::time_point expires_at;
    };

    auto sessions = std::make_shared<std::unordered_map<std::string, SessionState>>();
    auto mutex = std::make_shared<std::mutex>();
    auto counter = std::make_shared<std::atomic<unsigned long long>>(0);

    return [options, sessions, mutex, counter](Request& req, Response& res, Next next) {
        const auto now = std::chrono::steady_clock::now();
        auto id = req.cookie(options.cookie);

        {
            std::lock_guard<std::mutex> lock(*mutex);
            if (id.empty() || sessions->find(id) == sessions->end() || (*sessions)[id].expires_at <= now) {
                std::ostringstream generated;
                generated << "sess-" << counter->fetch_add(1, std::memory_order_relaxed);
                id = generated.str();
            }
            (*sessions)[id] = SessionState{now + std::chrono::seconds(options.maxAgeSeconds)};
        }

        res.cookie(options.cookie, id, options.maxAgeSeconds, true, options.secure, "Lax");
        next();
    };
}

} // namespace xp
