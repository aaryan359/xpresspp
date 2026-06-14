#include <xpresspp/xpresspp.h>
#include "src/routes/index.h"

// ─────────────────────────────────────────────────────────────────
//  Add more route files here, exactly like Express:
//
//  #include "src/routes/auth.h"    → app.use("/api/auth", routes::authRoutes());
//  #include "src/routes/users.h"   → app.use("/api/users", routes::userRoutes());
// ─────────────────────────────────────────────────────────────────

int main() {
    // Load .env file first — reads PORT, JWT_SECRET, DATABASE_URL, etc.
    xp::loadEnv();

    xp::App app;

    // ── Global Middleware ─────────────────────────────────────
    app.use(xp::logger());
    app.use(xp::cors());

    // ── Mount Routers ─────────────────────────────────────────
    // Same as: app.use('/api', indexRouter)  in Express
    app.use("/api", routes::indexRoutes());

    // Root
    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.html(R"(
            <h1>Xpress++ is running 🚀</h1>
            <p>Add routes in <code>src/routes/</code>.</p>
            <p>Mount them with <code>app.use("/prefix", routes::myRoutes())</code>.</p>
            <p>Run <code>xp watch</code> for live reload.</p>
        )");
    });

    // ── Start ─────────────────────────────────────────────────
    int port = xp::Config::envInt("PORT", 8080);
    app.listen(port);
    return 0;
}
