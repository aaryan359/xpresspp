#pragma once

#include "../router.h"
#include "session.h"

#include <string>
#include <openssl/crypto.h>

namespace xp {

struct CsrfOptions {
    std::string cookie = "csrf_token";
    std::string header = "x-csrf-token";
    std::string responseHeader = "x-csrf-token";
    int maxAgeSeconds = 3600;
    bool secure = false;
    std::string sameSite = "Strict";
    std::size_t tokenBytes = 32;
};

inline bool secureTokenEqual(const std::string& left, const std::string& right) {
    return !left.empty() && left.size() == right.size() &&
           CRYPTO_memcmp(left.data(), right.data(), left.size()) == 0;
}

inline Middleware csrf(CsrfOptions options = {}) {
    return [options](Request& req, Response& res, Next next) {
        const auto method = req.method();
        auto cookie_token = req.cookie(options.cookie);
        if (method == "GET" || method == "HEAD" || method == "OPTIONS") {
            if (cookie_token.empty()) {
                cookie_token = detail::secureRandomHex(options.tokenBytes);
                res.cookie(options.cookie, cookie_token, options.maxAgeSeconds,
                           false, options.secure, options.sameSite);
            }
            req.locals["csrfToken"] = cookie_token;
            if (!options.responseHeader.empty()) {
                res.header(options.responseHeader, cookie_token);
            }
            next();
            return;
        }

        const auto token = req.header(options.header);
        if (!secureTokenEqual(token, cookie_token)) {
            res.status(403).json({
                {"status", "error"},
                {"message", "Invalid CSRF token."}
            });
            return;
        }

        next();
    };
}

} // namespace xp
