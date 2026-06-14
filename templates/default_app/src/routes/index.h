#pragma once
#include <xpresspp/xpresspp.h>

// This is exactly like Express:
//   const router = express.Router()
//   router.get('/ping', handler)
//   module.exports = router
//
// Here:
//   inline xp::Router indexRoutes() { ... }
//   app.use("/api", indexRoutes());

namespace routes {

inline xp::Router indexRoutes() {
    xp::Router router;

    // GET /api/ping  — health check
    router.get("/ping", [](xp::Request& req, xp::Response& res) {
        res.json({{"status", "ok"}, {"message", "pong"}});
    });

    // GET /api/hello/:name  — URL params
    router.get("/hello/:name", [](xp::Request& req, xp::Response& res) {
        const auto name = req.param("name");
        res.json({{"hello", name}});
    });

    // POST /api/echo  — read JSON body
    router.post("/echo", [](xp::Request& req, xp::Response& res) {
        const auto body = req.json();
        res.json(body);
    });

    return router;
}

} 
