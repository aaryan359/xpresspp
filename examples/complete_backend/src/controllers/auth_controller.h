#pragma once
#include <xpresspp/xpresspp.h>
#include "../middleware/auth.h"
#include "../models/db.h"
#include <iostream>

using namespace std;

class AuthController {
public:
    // Registers a new user account (POST /api/auth/signup)
    static auto signup(xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            
            if (!body.isMember("username") || !body.isMember("password")) {
                res.status(400).json({{"error", "Username and password are required"}});
                co_return;
            }
            
            auto username = body["username"].asString();
            auto password = body["password"].asString();
            
            if (username.empty() || password.empty()) {
                res.status(400).json({{"error", "Username and password cannot be empty"}});
                co_return;
            }
            
            // Uses the clean Prisma-like Client API
            await prisma.user.create(username, password);
            
            res.json({
                {"success", true},
                {"message", "User registered successfully"}
            });
            
        } catch (const std::exception& e) {
            std::cerr << "[Auth Signup Error] " << e.what() << std::endl;
            res.status(400).json({
                {"error", "Registration failed (username may already be taken)"}
            });
        }
    }

    // Logs in a user and returns a JWT token (POST /api/auth/login)
    static auto login(xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            
            if (!body.isMember("username") || !body.isMember("password")) {
                res.status(400).json({{"error", "Username and password are required"}});
                co_return;
            }
            
            auto username = body["username"].asString();
            auto password = body["password"].asString();
            
            // Fetch the user object from the Model
            auto user = await prisma.user.findByUsername(username);
            
            if (user.isNull() || user["password"].asString() != password) {
                res.status(401).json({{"error", "Invalid username or password"}});
                co_return;
            }
            
            auto token = xp::generateJwt(username, AuthMiddleware::getSecret());
            res.json({
                {"success", true},
                {"token", token}
            });
        } catch (const std::exception& e) {
            std::cerr << "[Auth Login Error] " << e.what() << std::endl;
            res.status(500).json({{"error", "An internal error occurred during login"}});
        }
    }
};
