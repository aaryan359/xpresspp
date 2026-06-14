#pragma once
#include <xpresspp/xpresspp.h>

namespace middleware {

inline xp::Middleware requireAuth(const std::string& jwtSecret) {
    return xp::jwtAuth(jwtSecret);
}

} // namespace middleware
