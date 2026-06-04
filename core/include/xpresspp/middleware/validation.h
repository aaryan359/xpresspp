#pragma once

#include "../router.h"

#include <functional>
#include <string>
#include <utility>

namespace xp {

using Validator = std::function<std::string(Request&)>;

inline Middleware validate(Validator validator) {
    return [validator = std::move(validator)](Request& req, Response& res, Next next) {
        const auto error = validator(req);
        if (!error.empty()) {
            res.badRequest(error);
            return;
        }
        next();
    };
}

} // namespace xp
