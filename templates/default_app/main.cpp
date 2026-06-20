#include <xpresspp/xpresspp.h>
#include <iostream>

int main() {
    xp::loadEnv();
    xp::App app;

    app.use(xp::logger());
    app.use(xp::cors());

    // Configure database connection URL (Change to postgresql or mongodb if using docker)
    app.database("sqlite3://test.db");

    // Sync schema on startup
    app.onStart([]() async {
        try {
            await SchemaSync::syncAll();
            std::cout << "[DB] Schema sync completed successfully." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[DB Error] Sync failed: " << e.what() << std::endl;
        }
    });

    // 1. Root index endpoint
    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.ok({{"message", "Hello from Xpress++"}});
    });

    // 2. Fetch all users (eager loading their posts)
    app.get("/users", [](xp::Request& req, xp::Response& res) async {
        try {
            xp::obj query = {
                {"include", xp::obj{{"posts", true}}}
            };
            auto users = await prisma.user.findMany(query);
            res.ok(users);
        } catch (const std::exception& e) {
            res.serverError(e.what());
        }
    });

    // 3. Create a new user
    app.post("/users", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            if (body["username"].isNull()) {
                res.badRequest("Missing username");
                co_return;
            }
            await prisma.user.create(body);
            res.created({{"success", true}});
        } catch (const std::exception& e) {
            res.serverError(e.what());
        }
    });

    // 4. Fetch all posts (eager loading their author)
    app.get("/posts", [](xp::Request& req, xp::Response& res) async {
        try {
            xp::obj query = {
                {"include", xp::obj{{"author", true}}}
            };
            auto posts = await prisma.post.findMany(query);
            res.ok(posts);
        } catch (const std::exception& e) {
            res.serverError(e.what());
        }
    });

    // 5. Create a new post
    app.post("/posts", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            if (body["title"].isNull() || body["authorId"].isNull()) {
                res.badRequest("Missing title or authorId");
                co_return;
            }
            await prisma.post.create(body);
            res.created({{"success", true}});
        } catch (const std::exception& e) {
            res.serverError(e.what());
        }
    });

    app.listen();
    return 0;
}
