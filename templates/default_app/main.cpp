#include <xpresspp/xpresspp.h>
#include <stdexcept>

int main() {
    xp::App app;

    app.get("/api/ping", [](xp::Request& req, xp::Response& res) {
        res.json({
            {"status", "healthy"},
            {"framework", "Xpress++"}
        });
    });

    app.listen(8080);
    return 0;
}
