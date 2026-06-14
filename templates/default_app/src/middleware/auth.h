#pragma once
#include <xpresspp/xpresspp.h>

namespace middleware {

// Example auth middleware using req.locals to pass user data downstream.
//
// Usage:
//   app.get("/dashboard", middleware::requireAuth, handler);
//
// In the handler:
//   auto username = req.local<std::string>("username");

inline xp::Middleware requireAuth(const std::string& jwtSecret) {
    return xp::jwtAuth(jwtSecret);
}

} // namespace middleware
