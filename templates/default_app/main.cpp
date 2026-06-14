#include <xpresspp/xpresspp.h>

// ── Route Imports ─────────────────────────────────────────────
// Each #include is like: import { authRoutes } from './routes/auth'
// The function name tells you exactly which file it came from.
//
#include "src/routes/index.h"    // → indexRoutes()
// #include "src/routes/auth.h"  // → authRoutes()
// #include "src/routes/users.h" // → userRoutes()
// ─────────────────────────────────────────────────────────────

int main() {
    xp::loadEnv(); // reads .env → PORT, JWT_SECRET, etc.

    xp::App app;

    // ── Middleware ────────────────────────────────────────────
    app.use(xp::logger());
    app.use(xp::cors());

    // ── Mount Routes ──────────────────────────────────────────
    // Read as: "mount indexRoutes (src/routes/index.h) at /api"
    app.use("/api",       indexRoutes());
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
