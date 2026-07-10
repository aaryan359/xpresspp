#include <xpresspp/xpresspp.h>
#include <iostream>
#include <string>

int main() {
    xp::loadEnv();
    xp::App app;

    app.use(xp::logger());
    app.use(xp::cors());

    // Connect to PostgreSQL database using environment variable or direct URL
    app.database("postgresql://postgres:password@127.0.0.1:5450/xpresspp");

    // GET / - Root route
    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.ok({{"message", "Hello from Xpress++ Premium Backend!"}});
    });

    // GET /users - Get all users
    app.get("/users", [](xp::Request& req, xp::Response& res) async {
        auto users = await xpd.user.findMany();
        res.ok(users);
    });

    // POST /users - Create user
    app.post("/users", [](xp::Request& req, xp::Response& res) async {
        auto body = req.json();
        if (!body.isMember("username") || !body.isMember("email")) {
            res.status(400).json({{"error", "username and email are required"}});
            co_return;
        }
        await xpd.user.create(body);
        res.status(201).json({{"message", "User created"}});
    });

    // GET /users/:id - Get user by ID (optionally including posts)
    app.get("/users/:id", [](xp::Request& req, xp::Response& res) async {
        std::string id_str = req.param("id");
        int64_t id = std::stoll(id_str);

        Json::Value query;
        query["where"]["id"] = id;

        // Check if we should include posts
        if (req.query("include") == "posts") {
            query["include"]["posts"] = true;
        }

        auto user = await xpd.user.findUnique(query);
        if (user.isNull()) {
            res.status(404).json({{"error", "User not found"}});
        } else {
            res.ok(user);
        }
    });

    // POST /posts - Create post
    app.post("/posts", [](xp::Request& req, xp::Response& res) async {
        auto body = req.json();
        if (!body.isMember("title") || !body.isMember("content") || !body.isMember("authorId")) {
            res.status(400).json({{"error", "title, content, and authorId are required"}});
            co_return;
        }
        await xpd.post.create(body);
        res.status(201).json({{"message", "Post created"}});
    });

    // POST /transaction-demo - Demo ACID transaction and row locking
    app.post("/transaction-demo", [](xp::Request& req, xp::Response& res) async {
        auto body = req.json();
        if (!body.isMember("userId") || !body.isMember("newRole")) {
            res.status(400).json({{"error", "userId and newRole are required"}});
            co_return;
        }

        int64_t userId = body["userId"].asInt64();
        std::string newRole = body["newRole"].asString();

        std::cout << "[ACID Demo] Starting transaction..." << std::endl;
        auto tx = await beginTransaction();

        bool success = false;
        std::string err_msg;
        try {
            // Find user using FOR UPDATE lock to guarantee isolation/locking
            Json::Value lockQuery;
            lockQuery["where"]["id"] = userId;
            lockQuery["forUpdate"] = true;

            std::cout << "[ACID Demo] Fetching user " << userId << " with lock (FOR UPDATE)..." << std::endl;
            auto user = await tx.user.findUnique(lockQuery);

            if (user.isNull()) {
                std::cout << "[ACID Demo] User not found. Rolling back transaction." << std::endl;
                await tx.rollback();
                res.status(404).json({{"error", "User not found"}});
                co_return;
            }

            // Simulate business logic validation: if role is "fail", trigger explicit rollback to show ACID
            if (newRole == "fail") {
                std::cout << "[ACID Demo] Invalid role requested. Rolling back transaction." << std::endl;
                await tx.rollback();
                res.status(400).json({{"error", "Simulated business validation failed, transaction rolled back"}});
                co_return;
            }

            // Update user role inside the transaction
            Json::Value updateWhere;
            updateWhere["id"] = userId;
            Json::Value updateData;
            updateData["role"] = newRole;

            std::cout << "[ACID Demo] Updating user role to '" << newRole << "' inside transaction..." << std::endl;
            await tx.user.update(updateWhere, updateData);

            // Commit transaction
            std::cout << "[ACID Demo] Committing transaction..." << std::endl;
            await tx.commit();
            success = true;

            res.ok({{"message", "Transaction committed successfully"}, {"userId", userId}, {"newRole", newRole}});
        } catch (const std::exception& e) {
            std::cerr << "[ACID Demo] Exception encountered: " << e.what() << std::endl;
            err_msg = e.what();
        }

        if (!success && err_msg.length() > 0) {
            std::cout << "[ACID Demo] Rolling back transaction due to exception..." << std::endl;
            await tx.rollback();
            res.status(500).json({{"error", std::string("Internal error: ") + err_msg}});
        }
    });

    app.listen();
    return 0;
}
