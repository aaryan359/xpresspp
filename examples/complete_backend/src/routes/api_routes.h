#pragma once
#include <xpresspp/xpresspp.h>
#include "../controllers/auth_controller.h"
#include "../controllers/user_controller.h"
#include "../middleware/auth.h"
#include "../middleware/logger.h"

class ApiRoutes {
public:
    // Configures and binds all API endpoints to the Xpress++ application
    static void setup(xp::App& app) {
        // 1. Register Global Logger Middleware
        app.use(LoggerMiddleware::use());

        // 2. Public Index Route
        app.get("/", [](xp::Request& req, xp::Response& res) {
            res.html(
                "<h1>Xpress++ MVC Backend (Class-Based)</h1>"
                "<p>A structured, high-performance web service using modern C++20 class-based routing.</p>"
                "<ul>"
                "  <li><code>POST /api/auth/signup</code> - Register a user</li>"
                "  <li><code>POST /api/auth/login</code> - Get JWT Authentication Token</li>"
                "  <li><code>GET /api/users</code> - List all users (Protected by JWT)</li>"
                "  <li><code>GET /api/users/profile</code> - View active profile (Protected by JWT)</li>"
                "</ul>"
            );
        });

        // 3. Auth Routes (Signup & Login)
        app.post("/api/auth/signup", [](xp::Request& req, xp::Response& res) async {
            await AuthController::signup(req, res);
        });

        app.post("/api/auth/login", [](xp::Request& req, xp::Response& res) async {
            await AuthController::login(req, res);
        });

        // 4. User Profile & Listing Routes (Protected by JWT Auth Middleware)
        auto auth = AuthMiddleware::requireAuth();

        app.get("/api/users", auth, [](xp::Request& req, xp::Response& res) async {
            await UserController::getUsers(req, res);
        });

        app.get("/api/users/profile", auth, [](xp::Request& req, xp::Response& res) async {
            await UserController::getProfile(req, res);
        });
    }
};
