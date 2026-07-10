#pragma once

#include "../router.h"

#include <algorithm>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

namespace xp {

struct RateLimitOptions {
    int windowMs = 60000;
    int max = 100;
    std::string message = "Too many requests. Please try again later.";
    bool standardHeaders = true;
    std::size_t maxBuckets = 100000;
    std::function<std::string(Request&)> keyGenerator = [](Request& req) { return req.ip(); };
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
        const auto key = options.keyGenerator ? options.keyGenerator(req) : req.ip();

        {
            std::lock_guard<std::mutex> lock(*mutex);
            for (auto it = buckets->begin(); it != buckets->end();) {
                if (now - it->second.started_at > window) it = buckets->erase(it);
                else ++it;
            }
            if (buckets->size() >= options.maxBuckets && buckets->find(key) == buckets->end()) {
                res.status(503).json({
                    {"status", "error"},
                    {"message", "Rate limiter capacity reached."}
                });
                return;
            }

            auto& bucket = (*buckets)[key];
            if (bucket.count == 0 || now - bucket.started_at > window) {
                bucket.started_at = now;
                bucket.count = 0;
            }

            ++bucket.count;
            const auto remaining = std::max(0, options.max - bucket.count);
            const auto reset_seconds = std::max<long long>(1,
                std::chrono::duration_cast<std::chrono::seconds>(
                    bucket.started_at + window - now).count());
            if (options.standardHeaders) {
                res.header("RateLimit-Limit", std::to_string(options.max));
                res.header("RateLimit-Remaining", std::to_string(remaining));
                res.header("RateLimit-Reset", std::to_string(reset_seconds));
            }
            if (bucket.count > options.max) {
                res.header("Retry-After", std::to_string(reset_seconds));
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
