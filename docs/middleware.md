# Middleware

Middleware functions run **before** your route handler. They can inspect or modify the request and response, then either call `next()` to continue, or send a response directly.

## How middleware works

```cpp
app.use([](xp::Request& req, xp::Response& res, xp::Next next) {
    // 1. This runs before the route handler
    std::cout << "Incoming: " << req.method() << " " << req.path() << "\n";

    next();  // 2. Call next() to continue to the next middleware / route

    // 3. Code here runs AFTER the route handler finishes
    std::cout << "Response sent with status " << res.statusCode() << "\n";
});
```

::: warning Always call next() or send a response
Every middleware must either call `next()` or send a response. If it does neither, Xpress++ throws a `MiddlewareError` with a helpful hint.
:::

---

## Global middleware

Use `app.use()` to register middleware that runs on **every request**:

```cpp
app.use(xp::logger());          // Log all requests
app.use(xp::cors());            // Allow cross-origin requests
app.use(xp::securityHeaders()); // Set security headers
app.use(xp::requestId());       // Attach a request ID
```

**Order matters** — middleware runs in the order it's registered:

```cpp
app.use(xp::requestId());    // 1st
app.use(xp::logger());       // 2nd
app.use(xp::rateLimit());    // 3rd
// ... route handlers run after all middleware
```

---

## Writing your own middleware

A middleware is just a function with three parameters:

```cpp
// Simple request timer
auto timer = [](xp::Request& req, xp::Response& res, xp::Next next) {
    const auto start = std::chrono::steady_clock::now();
    next();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();
    std::cout << req.path() << " took " << elapsed << "ms\n";
};

app.use(timer);
```

### Middleware that stops the chain

To reject a request without calling `next()`, send a response directly:

```cpp
auto requireApiKey = [](xp::Request& req, xp::Response& res, xp::Next next) {
    const auto key = req.header("x-api-key");
    if (key != "my-secret-key") {
        res.status(401).json({{"error", "Invalid API key"}});
        return;  // Don't call next() — request stops here
    }
    next();  // Key is valid, continue
};
```

### Middleware that modifies the request

You can attach data to the request using custom headers or params:

```cpp
auto setUserId = [](xp::Request& req, xp::Response& res, xp::Next next) {
    // Decode JWT and get user ID...
    req.setParam("userId", "42");
    next();
};

app.use(setUserId);

// Later in a route handler:
app.get("/profile", [](xp::Request& req, xp::Response& res) {
    const auto userId = req.param("userId");  // "42"
    res.json({{"userId", userId}});
});
```

---

## Route-level middleware

Apply middleware to a single route only:

```cpp
app.get("/dashboard",
    {xp::apiKeyAuth("my-key")},
    [](xp::Request& req, xp::Response& res) {
        res.send("Welcome to the dashboard!");
    }
);
```

Multiple middleware for one route:

```cpp
app.post("/api/data",
    {xp::rateLimit({.max = 10}), xp::apiKeyAuth("secret")},
    [](xp::Request& req, xp::Response& res) {
        res.json({{"status", "ok"}});
    }
);
```

---

## Prefix-scoped middleware

Mount a sub-router or middleware only for paths that start with a prefix:

```cpp
xp::Router apiRouter;
apiRouter.get("/ping", [](xp::Request& req, xp::Response& res) {
    res.json({{"status", "ok"}});
});

// All /api/* routes will go through apiRouter
app.use("/api", apiRouter);
```

---

## After handlers

Run code after **every** response has been sent (for cleanup, logging, analytics):

```cpp
app.after([](xp::Request& req, xp::Response& res) {
    // This runs after the response has been sent to the client
    // Useful for metrics, audit logs, cleanup
    logToAnalytics(req.method(), req.path(), res.statusCode());
});
```

---

## Built-in middleware

Xpress++ ships with the following middleware out of the box:

| Middleware | Function | Docs |
|------------|----------|------|
| `xp::logger()` | Color-coded request logging | [Logging](/logging) |
| `xp::cors()` | CORS headers | [CORS](/cors) |
| `xp::rateLimit()` | IP-based rate limiting | [Rate limit](/rate-limit) |
| `xp::apiKeyAuth()` | API key authentication | [Auth](/authentication) |
| `xp::basicAuth()` | HTTP Basic authentication | [Auth](/authentication) |
| `xp::securityHeaders()` | Security headers (XSS, CSP, etc.) | [Security](/security) |
| `xp::csrf()` | CSRF token verification | [CSRF](/csrf) |
| `xp::session()` | Cookie-based sessions | [Session](/session) |
| `xp::requestId()` | Attach unique request IDs | [Request ID](/request-id) |
| `xp::bodyLimit()` | Reject oversized bodies | [Body limit](/body-limit) |
| `xp::validate()` | Custom request validation | [Validation](/validation) |
