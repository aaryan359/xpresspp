#include <xpresspp/xpresspp.h>
#include <stdexcept>

int main() {
    xp::App app;

    // ── Healthcheck ──────────────────────────────────────────
    app.get("/api/ping", [](xp::Request& req, xp::Response& res) {
        res.json({
            {"status",    "healthy"},
            {"framework", "Xpress++"}
        });
    });

    // ── Example: typed errors ────────────────────────────────
    // Throw xp::BadRequestError, xp::NotFoundError, etc. from
    // any handler and xpress++ will return the right HTTP status.
    //
    // app.get("/api/users/:id", [](xp::Request& req, xp::Response& res) {
    //     const auto id = req.param("id");
    //     if (id.empty()) {
    //         throw xp::BadRequestError("id parameter is required");
    //     }
    //     // ... fetch from DB ...
    //     throw xp::NotFoundError("User " + id + " not found");
    // });

    app.listen(8080);
    return 0;
}
