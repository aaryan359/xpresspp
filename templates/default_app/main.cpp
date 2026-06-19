#include <xpresspp/xpresspp.h>

int main() {
    xp::loadEnv();

    xp::App app;

    app.use(xp::logger());
    app.use(xp::cors());

    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.ok({{"message", "Hello from Xpress++"}});
    });

    app.get("/api/ping", [](xp::Request& req, xp::Response& res) {
        res.ok({{"pong", true}});
    });

    app.listen();
    return 0;
}
