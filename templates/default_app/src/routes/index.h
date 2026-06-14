#pragma once
#include <xpresspp/xpresspp.h>
#include "../controllers/home_controller.h"

namespace routes {

inline void registerIndex(xp::Router& r) {
    // GET /api/ping  — health check
    r.get("/ping", [](xp::Request& req, xp::Response& res) {
        res.json({{"status", "ok"}, {"message", "pong"}});
    });

    // GET /api/hello/:name  — route params
    r.get("/hello/:name", [](xp::Request& req, xp::Response& res) {
        const auto name = req.param("name");
        res.json({{"hello", name}});
    });

    // POST /api/echo  — read JSON body
    r.post("/echo", [](xp::Request& req, xp::Response& res) {
        const auto body = req.json();
        res.json(body);
    });
}

} // namespace routes
