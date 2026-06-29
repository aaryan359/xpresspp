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

    // ── To add database support ───────────────────────────────────────────────
    //
    //  1. Edit schema.xp to define your models
    //  2. Run:  xp migrate
    //  3. Connect inside main():
    //       app.database("sqlite3://app.db");
    //  4. Then use the xpd client in async routes:
    //       app.get("/users", [](xp::Request& req, xp::Response& res) async {
    //           auto users = await xpd.user.findMany();
    //           res.ok(users);
    //       });
    // ─────────────────────────────────────────────────────────────────────────

    app.listen();
    return 0;
}
