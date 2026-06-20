#include <xpresspp/xpresspp.h>
#include <xpresspp/test.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    xp::loadEnv();

    xp::App app;

    app.use(xp::logger());
    app.use(xp::cors());

    // Configure database connection URL
    app.database("sqlite3://test.db");

    // Sync schema and optionally run tests on startup
    app.onStart([&app]() -> drogon::Task<void> {
        // Sync database tables
        co_await SchemaSync::syncAll();

        const char* run_tests = std::getenv("RUN_TESTS");
        if (run_tests && std::string(run_tests) == "true") {
            std::thread([&app]() {
                try {
                    // Give Drogon event loops a brief moment to start
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));

                    std::cout << "[TEST] Starting TestClient assertions..." << std::endl;
                    auto client = xp::request(app);

                    // 1. Create a user
                    std::cout << "[TEST] Sending POST /users..." << std::endl;
                    xp::Response res1;
                    try {
                        res1 = client.post("/users").send(xp::obj{{"username", "alice"}});
                    } catch (const std::exception& e) {
                        std::cerr << "[TEST] client.post(/users).send failed: " << e.what() << std::endl;
                        throw;
                    }
                    std::cout << "[TEST] Create user response status: " << res1.statusCode() << std::endl;
                    std::cout << "[TEST] Create user response body: " << res1.body() << std::endl;
                    assert(res1.statusCode() == 201);

                    // Get the user ID
                    std::cout << "[TEST] Sending GET /users..." << std::endl;
                    auto users_res = client.get("/users").send();
                    std::cout << "[TEST] Get users response status: " << users_res.statusCode() << std::endl;
                    std::cout << "[TEST] Get users response body: " << users_res.body() << std::endl;
                    assert(users_res.statusCode() == 200);
                    auto users_json = users_res.json();
                    assert(users_json.isArray());
                    assert(users_json.size() == 1);
                    int alice_id = users_json[0]["id"].asInt();
                    std::cout << "[TEST] Found user ID: " << alice_id << std::endl;

                    // 2. Create posts for user
                    std::cout << "[TEST] Sending POST /posts 1..." << std::endl;
                    auto res2 = client.post("/posts").send(xp::obj{
                        {"title", "My First Post"},
                        {"authorId", alice_id}
                    });
                    std::cout << "[TEST] Create post 1 response: " << res2.body() << std::endl;
                    assert(res2.statusCode() == 201);

                    std::cout << "[TEST] Sending POST /posts 2..." << std::endl;
                    auto res3 = client.post("/posts").send(xp::obj{
                        {"title", "My Second Post"},
                        {"authorId", alice_id}
                    });
                    std::cout << "[TEST] Create post 2 response: " << res3.body() << std::endl;
                    assert(res3.statusCode() == 201);

                    // 3. Eager load posts for user
                    std::cout << "[TEST] Sending GET /users (eager loading posts)..." << std::endl;
                    auto users_with_posts_res = client.get("/users").send();
                    std::cout << "[TEST] Eager loaded users response: " << users_with_posts_res.body() << std::endl;
                    assert(users_with_posts_res.statusCode() == 200);
                    auto users_with_posts = users_with_posts_res.json();
                    assert(users_with_posts[0]["posts"].isArray());
                    assert(users_with_posts[0]["posts"].size() == 2);
                    assert(users_with_posts[0]["posts"][0]["title"].asString() == "My First Post");

                    // 4. Eager load author for posts
                    std::cout << "[TEST] Sending GET /posts (eager loading author)..." << std::endl;
                    auto posts_with_author_res = client.get("/posts").send();
                    std::cout << "[TEST] Eager loaded posts response: " << posts_with_author_res.body() << std::endl;
                    assert(posts_with_author_res.statusCode() == 200);
                    auto posts_with_author = posts_with_author_res.json();
                    assert(posts_with_author[0]["author"].isObject());
                    assert(posts_with_author[0]["author"]["username"].asString() == "alice");

                    std::cout << "[TEST] ALL assertions passed successfully!" << std::endl;
                    drogon::app().quit();
                } catch (const std::exception& e) {
                    std::cerr << "[TEST] Assertion failed inside test thread: " << e.what() << std::endl;
                    drogon::app().quit();
                }
            }).detach();
        }
        co_return;
    });

    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.ok({{"message", "Hello from Xpress++"}});
    });

    app.post("/users", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            co_await prisma.user.create(body);
            res.created({{"success", true}});
        } catch (const std::exception& e) {
            std::cerr << "[APP] /users post failed: " << e.what() << std::endl;
            res.serverError(e.what());
        }
    });

    app.post("/posts", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            co_await prisma.post.create(body);
            res.created({{"success", true}});
        } catch (const std::exception& e) {
            std::cerr << "[APP] /posts post failed: " << e.what() << std::endl;
            res.serverError(e.what());
        }
    });

    app.get("/users", [](xp::Request& req, xp::Response& res) async {
        try {
            xp::obj query = {
                {"include", xp::obj{{"posts", true}}}
            };
            auto users = co_await prisma.user.findMany(query);
            res.ok(users);
        } catch (const std::exception& e) {
            std::cerr << "[APP] /users get failed: " << e.what() << std::endl;
            res.serverError(e.what());
        }
    });

    app.get("/posts", [](xp::Request& req, xp::Response& res) async {
        try {
            xp::obj query = {
                {"include", xp::obj{{"author", true}}}
            };
            auto posts = co_await prisma.post.findMany(query);
            res.ok(posts);
        } catch (const std::exception& e) {
            std::cerr << "[APP] /posts get failed: " << e.what() << std::endl;
            res.serverError(e.what());
        }
    });

    app.listen();
    return 0;
}
