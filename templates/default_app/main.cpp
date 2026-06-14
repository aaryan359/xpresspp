#include <xpresspp/xpresspp.h>
#include "src/routes/index.h"

int main() {
    // Load .env file before anything else
    xp::loadEnv();

    xp::App app;

    // ── Middleware ────────────────────────────────────────────
    app.use(xp::logger());
    app.use(xp::cors());

    // ── Routes ───────────────────────────────────────────────
    app.group("/api", [](xp::Router& r) {
        routes::registerIndex(r);
    });

    // Root
    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.html(R"(
            <h1>Xpress++ is running 🚀</h1>
            <p>Edit <code>src/routes/index.h</code> to add your routes.</p>
            <p>Run <code>xp watch</code> for live reload.</p>
        )");
    });

    // ── Start ─────────────────────────────────────────────────
    int port = xp::Config::envInt("PORT", 8080);
    app.listen(port);
    return 0;
}
