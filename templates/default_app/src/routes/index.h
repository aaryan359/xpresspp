#pragma once
#include <xpresspp/xpresspp.h>


inline xp::Router indexRoutes() {
    xp::Router router;

    // GET /api/ping
    router.get("/ping", [](xp::Request& req, xp::Response& res) {
        res.json({{"status", "ok"}, {"message", "pong"}});
    });

    // GET /api/hello/:name
    router.get("/hello/:name", [](xp::Request& req, xp::Response& res) {
        res.json({{"hello", req.param("name")}});
    });

    // POST /api/echo
    router.post("/echo", [](xp::Request& req, xp::Response& res) {
        res.json(req.json());
    });

    return router;
}
