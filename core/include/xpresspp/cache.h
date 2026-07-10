#pragma once

#include "utils.h"

#include <chrono>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace xp {

// Fast process-local TTL cache. No serialization, socket, or database overhead.
// Use a distributed adapter instead when values must be shared across processes.
class MemoryCache {
    using Clock = std::chrono::steady_clock;
    struct Entry {
        xp::var value;
        std::optional<Clock::time_point> expiresAt;
    };

    mutable std::shared_mutex mutex_;
    mutable std::unordered_map<std::string, Entry> entries_;

    void removeIfExpired(const std::string& key, Clock::time_point now) const {
        auto it = entries_.find(key);
        if (it != entries_.end() && it->second.expiresAt && *it->second.expiresAt <= now) {
            entries_.erase(it);
        }
    }

public:
    void set(std::string key, xp::var value, std::chrono::seconds ttl = std::chrono::seconds{0}) {
        std::optional<Clock::time_point> expiry;
        if (ttl.count() > 0) expiry = Clock::now() + ttl;
        std::unique_lock lock(mutex_);
        entries_.insert_or_assign(std::move(key), Entry{std::move(value), expiry});
    }

    void set(std::string key, xp::var value, int ttlSeconds) {
        set(std::move(key), std::move(value), std::chrono::seconds{ttlSeconds});
    }

    std::optional<xp::var> get(const std::string& key) const {
        const auto now = Clock::now();
        {
            std::shared_lock lock(mutex_);
            const auto it = entries_.find(key);
            if (it == entries_.end()) return std::nullopt;
            if (!it->second.expiresAt || *it->second.expiresAt > now) return it->second.value;
        }
        std::unique_lock lock(mutex_);
        removeIfExpired(key, now);
        return std::nullopt;
    }

    xp::var getOr(const std::string& key, xp::var fallback = {}) const {
        auto value = get(key);
        return value ? *value : std::move(fallback);
    }

    bool has(const std::string& key) const { return get(key).has_value(); }

    bool remove(const std::string& key) {
        std::unique_lock lock(mutex_);
        return entries_.erase(key) > 0;
    }

    std::size_t sweep() {
        const auto now = Clock::now();
        std::unique_lock lock(mutex_);
        const auto before = entries_.size();
        for (auto it = entries_.begin(); it != entries_.end();) {
            if (it->second.expiresAt && *it->second.expiresAt <= now) it = entries_.erase(it);
            else ++it;
        }
        return before - entries_.size();
    }

    void clear() {
        std::unique_lock lock(mutex_);
        entries_.clear();
    }

    std::size_t size() const {
        std::shared_lock lock(mutex_);
        return entries_.size();
    }
};

inline MemoryCache xpc;

} // namespace xp
