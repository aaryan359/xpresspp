#pragma once

#include "../router.h"

#include <cstddef>

namespace xp {

inline Middleware bodyLimit(std::size_t max_bytes) {
    return [max_bytes](Request& req, Response& res, Next next) {
        if (req.body().size() > max_bytes) {
            res.status(413).json({
                {"status", "error"},
                {"message", "Request body is too large."}
            });
            return;
        }
        next();
    };
}

} // namespace xp
