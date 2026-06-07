#include <xpresspp/xpresspp.h>
#include <thread>
#include <algorithm>

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
    
    // Performance optimizations for high load
    drogon::app().enableReusePort(true);
    drogon::app().setThreadNum(0); // 0 means use all available CPU cores
    drogon::app().enableServerHeader(false);
    drogon::app().enableDateHeader(false);
    
    app.listen(8085);
    return 0;
}
