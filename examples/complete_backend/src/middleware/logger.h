#pragma once
#include <xpresspp/xpresspp.h>
#include <iostream>

class LoggerMiddleware {
public:
    static xp::Middleware use() {
        return [](xp::Request& req, xp::Response& res, xp::Next next) {
            std::cout << "[Request] " << req.method() << " " << req.path() 
                      << " from " << req.ip() << std::endl;
            next();
        };
    }
};
