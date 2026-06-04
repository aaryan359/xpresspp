#pragma once

#include "../router.h"

#include <string>

namespace xp {

struct CsrfOptions {
    std::string cookie = "csrf_token";
    std::string header = "x-csrf-token";
};

inline Middleware csrf(CsrfOptions options = {}) {
    return [options](Request& req, Response& res, Next next) {
        const auto method = req.method();
        if (method == "GET" || method == "HEAD" || method == "OPTIONS") {
            next();
            return;
        }

        const auto token = req.header(options.header);
        if (token.empty() || token != req.cookie(options.cookie)) {
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
