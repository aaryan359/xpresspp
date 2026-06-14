#pragma once
#include <xpresspp/xpresspp.h>

namespace controllers {

// Example of how to use req.locals to pass data from middleware to handlers.
//
// In a middleware:
//   req.locals["user"] = std::string("alice");
//
// In a handler:
//   auto user = req.local<std::string>("user");

inline void home(xp::Request& req, xp::Response& res) {
    res.json({
        {"message", "Welcome to Xpress++"},
        {"version", "1.0.0"}
    });
}

} // namespace controllers
