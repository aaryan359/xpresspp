#pragma once
#include <xpresspp/xpresspp.h>

struct Home {
    static void home(xp::Request& req, xp::Response& res) {
        res.json({
            {"message", "Welcome to Xpress++"},
            {"version", "1.0.0"}
        });
    }
};
