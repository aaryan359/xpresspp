#include <xpresspp/xpresspp.h>


int main() {
    xp::loadEnv();

    xp::App app;

    app.use(xp::logger());
    app.use(xp::cors());

    // Configure database connection URL
    app.database("postgresql://postgres:postgres@localhost:5432/testdb");

    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.ok({{"message", "Hello from Xpress++"}});
    });

    app.get("/users", [](xp::Request& req, xp::Response& res) async {
        try {
            // Find active users with age > 18 using direct JSON initializer list
            xp::obj query = {
                {"where", xp::obj{
                    {"age", xp::obj{
                        {"gt", 18}
                    }},
                    {"status", "active"}
                }}
            };
            auto users = await prisma.user.findMany(query);
            res.ok(users);
        } catch (const std::exception& e) {
            res.serverError(e.what());
        }
    });

    app.post("/users", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            await prisma.user.create(body);
            res.created({{"success", true}});
        } catch (const std::exception& e) {
            res.serverError(e.what());
        }
    });

    app.listen();
    return 0;
}
