#include <xpresspp/xpresspp.h>
#include <cassert>
#include <iostream>
#include <string>
#include <thread>
#include <chrono>

using namespace std;

// JWT Secret Key
const string JWT_SECRET = "xpresspp-jwt-secret-key-12345";

int main() {


    xp::App app;


    xp::DbConfig db_config;
    db_config.driver = "postgresql";
    db_config.host = "127.0.0.1";
    db_config.port = 5450;
    db_config.database = "xpresspp";
    db_config.username = "postgres";
    db_config.password = "password";
    db_config.connection_number = 1;
    app.database(db_config);

    // Automatically create the users table when the application starts
    app.onStart([]() async {
        try {
            await xp::query(
                "CREATE TABLE IF NOT EXISTS users ("
                "id SERIAL PRIMARY KEY, "
                "username VARCHAR(50) UNIQUE NOT NULL, "
                "password VARCHAR(50) NOT NULL"
                ");"
            );
            cout << "[DB Setup] PostgreSQL 'users' table is ready." << endl;
        } catch (const exception& e) {
            cerr << "[DB Setup Error] Failed to verify/create users table: " << e.what() << endl;
        }
    });

    // 2. Token Verification Middleware (Built-in from Xpress++)
    auto tokenMiddleware = xp::jwtAuth(JWT_SECRET);

    // 3. Authentication Routes (Signup, Login, Protected Home)

    // POST /api/auth/signup - Register a new user
    app.post("/api/auth/signup", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            auto username = body["username"].asString();
            auto password = body["password"].asString();
            
            if (username.empty() || password.empty()) {
                res.status(400).json({{"error", "Username and password are required"}});
                co_return;
            }
            
            await xp::query(
                "INSERT INTO users (username, password) VALUES ($1, $2);",
                username, password
            );
            
            res.json({{"success", true}, {"message", "User registered successfully"}});
        } catch (const std::exception& e) {
            res.status(400).json({{"error", "Registration failed (username may already exist)"}});
        }
    });

    // POST /api/auth/login - Authenticate and return JWT token
    app.post("/api/auth/login", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            auto username = body["username"].asString();
            auto password = body["password"].asString();
            
            if (username.empty() || password.empty()) {
                res.status(400).json({{"error", "Username and password are required"}});
                co_return;
            }
            
            auto result = await xp::query(
                "SELECT password FROM users WHERE username = $1;",
                username
            );
            
            if (result.empty() || result[0]["password"].as<std::string>() != password) {
                res.status(401).json({{"error", "Invalid username or password"}});
                co_return;
            }
            
            auto token = xp::generateJwt(username, JWT_SECRET);
            res.json({
                {"success", true},
                {"token", token}
            });
        } catch (const std::exception& e) {
            res.status(500).json({{"error", e.what()}});
        }
    });

    // GET /api/home - Protected route requiring a valid JWT token via Bearer Auth
    app.get("/api/home", tokenMiddleware, [](xp::Request& req, xp::Response& res) {
        auto username = req.param("username");
        res.json({
            {"success", true},
            {"message", "Welcome to your secure home page, " + username + "!"}
        });
    });

    // Public index route
    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.html("<h1>Welcome to Xpress++ Auth API</h1><p>Running on PostgreSQL!</p>");
    });

    // Start server
    app.listen(8082);
    return 0;
}
