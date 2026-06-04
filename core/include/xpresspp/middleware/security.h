#pragma once

#include "../router.h"

#include <string>

namespace xp {

struct SecurityHeadersOptions {
    bool contentTypeOptions = true;
    bool frameOptions = true;
    bool referrerPolicy = true;
    bool xssProtection = true;
    bool hsts = false;
    std::string frameOptionsValue = "DENY";
    std::string referrerPolicyValue = "no-referrer";
    std::string hstsValue = "max-age=15552000; includeSubDomains";
};

inline Middleware securityHeaders(SecurityHeadersOptions options = {}) {
    return [options](Request& req, Response& res, Next next) {
        (void)req;
        if (options.contentTypeOptions) {
            res.header("X-Content-Type-Options", "nosniff");
        }
        if (options.frameOptions) {
            res.header("X-Frame-Options", options.frameOptionsValue);
        }
        if (options.referrerPolicy) {
            res.header("Referrer-Policy", options.referrerPolicyValue);
        }
        if (options.xssProtection) {
            res.header("X-XSS-Protection", "0");
        }
        if (options.hsts) {
            res.header("Strict-Transport-Security", options.hstsValue);
        }
        next();
    };
}

} // namespace xp
