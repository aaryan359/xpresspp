#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    app.get("/json", [](xp::Request&, xp::Response& res) {
        res.json({{"message", "Hello World"}});
    });

    app.get("/text", [](xp::Request&, xp::Response& res) {
        res.send("Hello World");
    });

    // Run in production mode with low verbosity
    drogon::app().setLogLevel(trantor::Logger::kWarn);
    
    app.listen(8080);
    return 0;
}
