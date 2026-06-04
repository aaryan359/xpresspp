#pragma once

#include "../router.h"

#include <string>

namespace xp {

struct CorsOptions {
    std::string origin = "*";
    std::string methods = "GET,POST,PUT,PATCH,DELETE,OPTIONS,HEAD";
    std::string headers = "Content-Type,Authorization";
    bool credentials = false;
};

inline Middleware cors(CorsOptions options = {}) {
    return [options](Request& req, Response& res, Next next) {
        res.header("Access-Control-Allow-Origin", options.origin)
           .header("Access-Control-Allow-Methods", options.methods)
           .header("Access-Control-Allow-Headers", options.headers);

        if (options.credentials) {
            res.header("Access-Control-Allow-Credentials", "true");
        }

        if (req.method() == "OPTIONS") {
            res.status(204).send("");
            return;
        }

        next();
    };
}

} // namespace xp
