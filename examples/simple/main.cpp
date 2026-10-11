#include <xpresspp/xpresspp.h>

int main() {
    xp::loadEnv();
    xp::App app;

    app.use(xp::logger());
    app.use(xp::cors());

    // ── Routes ────────────────────────────────────────────────────────────────

    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.ok({{"message", "Hello from Xpress++!"}});
    });

    app.get("/ping", [](xp::Request& req, xp::Response& res) {
        res.ok({{"status", "ok"}});
    });

    app.listen();
    return 0;
}
