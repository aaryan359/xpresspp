#pragma once

#include "../router.h"

#include <chrono>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#include <openssl/rand.h>

namespace xp {

struct SessionOptions {
    std::string cookie = "xpresspp_session";
    int maxAgeSeconds = 86400;
    bool secure = false;
    bool httpOnly = true;
    std::string sameSite = "Lax";
    std::size_t idBytes = 32;
    std::size_t maxSessions = 100000;
};

namespace detail {
inline std::string secureRandomHex(std::size_t byte_count) {
    if (byte_count < 16) {
        throw std::invalid_argument("Secure tokens must contain at least 16 random bytes.");
    }
    std::vector<unsigned char> bytes(byte_count);
    if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
        throw std::runtime_error("OpenSSL failed to generate cryptographically secure random bytes.");
    }

    static constexpr char hex[] = "0123456789abcdef";
    std::string result(bytes.size() * 2, '0');
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        result[i * 2] = hex[bytes[i] >> 4];
        result[i * 2 + 1] = hex[bytes[i] & 0x0f];
    }
    return result;
}
} // namespace detail

inline Middleware session(SessionOptions options = {}) {
    struct SessionState {
        std::chrono::steady_clock::time_point expires_at;
    };

    auto sessions = std::make_shared<std::unordered_map<std::string, SessionState>>();
    auto mutex = std::make_shared<std::mutex>();
    return [options, sessions, mutex](Request& req, Response& res, Next next) {
        const auto now = std::chrono::steady_clock::now();
        auto id = req.cookie(options.cookie);

        {
            std::lock_guard<std::mutex> lock(*mutex);
            for (auto it = sessions->begin(); it != sessions->end();) {
                if (it->second.expires_at <= now) it = sessions->erase(it);
                else ++it;
            }

            const auto existing = sessions->find(id);
            if (id.empty() || existing == sessions->end()) {
                if (sessions->size() >= options.maxSessions) {
                    throw std::runtime_error("Session capacity reached. Configure a persistent session store.");
                }
                do {
                    id = detail::secureRandomHex(options.idBytes);
                } while (sessions->find(id) != sessions->end());
            }
            (*sessions)[id] = SessionState{now + std::chrono::seconds(options.maxAgeSeconds)};
        }

        req.locals["sessionId"] = id;
        res.cookie(options.cookie, id, options.maxAgeSeconds,
                   options.httpOnly, options.secure, options.sameSite);
        next();
    };
}

} // namespace xp
