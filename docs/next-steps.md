# Next Steps

You've learned the basics of Xpress++. Here's where to go from here.

## Build a real API

Try building a complete REST API with all the pieces:

```cpp
#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    // ── Global middleware ───────────────────────────────
    app.use(xp::requestId());
    app.use(xp::logger());
    app.use(xp::cors({.origin = "https://my-frontend.com"}));
    app.use(xp::securityHeaders());
    app.use(xp::rateLimit({.windowMs = 60000, .max = 200}));

    // ── Public routes ───────────────────────────────────
    app.get("/health", [](xp::Request& req, xp::Response& res) {
        res.json({{"status", "ok"}});
    });

    // ── Protected routes ────────────────────────────────
    app.group("/api/v1", [](xp::Router& r) {
        r.get("/users", {xp::apiKeyAuth("secret")}, [](xp::Request& req, xp::Response& res) {
            res.json({{"users", {}}});
        });

        r.post("/users", {xp::apiKeyAuth("secret"), xp::bodyLimit(50 * 1024)},
            [](xp::Request& req, xp::Response& res) {
                const auto body = req.json();
                if (!body.isMember("name")) {
                    throw xp::BadRequestError("'name' is required.");
                }
                res.status(201).json({{"name", body["name"]}});
            }
        );
    });

    // ── Error handling ──────────────────────────────────
    app.onError([](const std::exception& e, xp::Request& req, xp::Response& res) {
        int status = 500;
        if (const auto* he = dynamic_cast<const xp::HttpError*>(&e)) {
            status = he->statusCode();
        }
        res.status(status).json({{"error", e.what()}});
    });

    // ── Start server ────────────────────────────────────
    xp::AppConfig config;
    config.port  = xp::Config::envInt("PORT", 8080);
    config.debug = xp::Config::envBool("DEBUG", true);
    app.configure(config);
    app.listen();
}
```

---

## Useful resources

- [Drogon documentation](https://github.com/drogonframework/drogon/wiki) — The HTTP engine underneath
- [jsoncpp documentation](https://open-source-parsers.github.io/jsoncpp-docs/doxygen/) — JSON API reference
- [C++20 reference](https://en.cppreference.com/w/cpp/20) — Standard library

---

## Contributing

Found a bug or want to add a feature? Pull requests are welcome!

1. Fork the repo
2. Create a branch: `git checkout -b feat/my-feature`
3. Make your changes
4. Test with `xp build` inside `templates/default_app`
5. Open a PR

---

## Getting help

- Open an issue on GitHub
- Check the sidebar for the relevant documentation page
- Run `xp doctor` to diagnose dependency issues
