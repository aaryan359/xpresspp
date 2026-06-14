#include <xpresspp/xpresspp.h>
#include "src/routes/index.h"    // → indexRoutes()

int main() {
    xp::loadEnv(); // reads .env → PORT, JWT_SECRET, etc.

    xp::App app;

    // ── Middleware ────────────────────────────────────────────
    app.use(xp::logger());
    app.use(xp::cors());

    // ── Global Error Handler ──────────────────────────────────
    app.onError([](const std::exception& e, xp::Request& req, xp::Response& res) {
        res.status(500).json({
            {"error", e.what()},
            {"path", req.path()}
        });
    });

    // ── Mount Routes ──────────────────────────────────────────
    // Read as: "mount indexRoutes (src/routes/index.h) at /api"
    app.use("/api", Routes());
    // app.use("/api/auth",  authRoutes());   ← uncomment when you add auth.h
    // app.use("/api/users", userRoutes());   ← uncomment when you add users.h

    // Root
    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.html(R"(
            <h1>Xpress++ 🚀</h1>
            <p>Add a file in <code>src/routes/</code>, include it above, mount it below.</p>
        )");
    });

    // ── Start ─────────────────────────────────────────────────
    app.listen(xp::Config::envInt("PORT", 8080));
    return 0;
}
