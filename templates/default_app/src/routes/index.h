#pragma once
#include <xpresspp/xpresspp.h>
#include "src/controllers/home_controller.h"

// Mount with: app.use("/api", indexRoutes());

inline xp::Router Routes() {

    xp::Router router;

    // GET /api/ping
    router.get("/ping", [](xp::Request& req, xp::Response& res) {
        res.json({{"status", "ok"}, {"message", "pong"}});
    });

    // GET /api/ — delegates to Home controller
    router.get("/h", Home::home);

    // GET /api/error-test
    router.get("/error-test", [](xp::Request& req, xp::Response& res) {
        throw std::runtime_error("Something went wrong!");
    });

    // GET /api/hello/:name
    router.get("/hello/:name", [](xp::Request& req, xp::Response& res) {
        res.json({{"hello", req.param("name")}});
    });

    // POST /api/echo
    router.post("/echo", [](xp::Request& req, xp::Response& res) {
        res.json(req.json());
    });

    // POST /api/signup — validation example
    router.post("/signup", [](xp::Request& req, xp::Response& res) {
        auto [body, err] = req.validate({
            {"username", xp::string().required("Username is required!").min(3, "Username must be 3+ chars!")},
            {"password", xp::string().required().min(8)}
        });
        if (err) {
            res.status(400).json(err);
            return;
        }
        res.json({{"status", "success"}, {"username", body["username"]}});
    });

    return router;
}
