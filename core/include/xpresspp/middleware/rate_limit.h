#pragma once

#include "../router.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace xp {

struct RateLimitOptions {
    int windowMs = 60000;
    int max = 100;
    std::string message = "Too many requests. Please try again later.";
};

inline Middleware rateLimit(RateLimitOptions options = {}) {
    using Clock = std::chrono::steady_clock;

    struct Bucket {
        Clock::time_point started_at;
        int count = 0;
    };

    auto buckets = std::make_shared<std::unordered_map<std::string, Bucket>>();
    auto mutex = std::make_shared<std::mutex>();

    return [options, buckets, mutex](Request& req, Response& res, Next next) {
        const auto now = Clock::now();
        const auto window = std::chrono::milliseconds(options.windowMs);
        const auto key = req.ip();

        {
            std::lock_guard<std::mutex> lock(*mutex);
            auto& bucket = (*buckets)[key];
            if (bucket.count == 0 || now - bucket.started_at > window) {
                bucket.started_at = now;
                bucket.count = 0;
            }

            ++bucket.count;
            if (bucket.count > options.max) {
                res.status(429).json({
                    {"status", "error"},
                    {"message", options.message}
                });
                return;
            }
        }

        next();
    };
}

} // namespace xp
