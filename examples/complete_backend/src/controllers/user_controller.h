#pragma once
#include <xpresspp/xpresspp.h>
#include "../models/user.h"
#include <iostream>

class UserController {
public:
    // Gets all registered users (GET /api/users, protected)
    static auto getUsers(xp::Request& req, xp::Response& res) async {
        try {
            // Retrieve all users as a clean JSON list using the User Model
            auto users = await User::findAll();
            res.json(users);
        } catch (const std::exception& e) {
            std::cerr << "[UserController getUsers Error] " << e.what() << std::endl;
            res.status(500).json({{"error", "Failed to retrieve user list"}});
        }
    }

    // Gets the profile details of the currently authenticated user (GET /api/users/profile, protected)
    static auto getProfile(xp::Request& req, xp::Response& res) async {
        try {
            auto username = req.param("username");
            if (username.empty()) {
                res.status(401).json({{"error", "Unauthorized: Incomplete auth payload"}});
                co_return;
            }
            
            // Fetch user profile info via Model
            auto user = await User::findByUsername(username);
            
            if (user.isNull()) {
                res.status(404).json({{"error", "User profile not found"}});
                co_return;
            }
            
            // Remove sensitive fields before returning to client
            user.removeMember("password");
            
            Json::Value response;
            response["success"] = true;
            response["profile"] = user;
            res.json(response);
            
        } catch (const std::exception& e) {
            std::cerr << "[UserController getProfile Error] " << e.what() << std::endl;
            res.status(500).json({{"error", "Failed to load user profile"}});
        }
    }
};
