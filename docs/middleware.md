---
title: Middleware
description: Run shared request and response logic before route handlers.
---

# Middleware

Middleware can inspect the request, modify the response, then call `next()`.

```cpp
app.use([](xp::Request& req, xp::Response& res, xp::Next next) {
    res.header("X-Powered-By", "Xpress++");
    next();
});
```

## Built-in middleware

```cpp
app.use(xp::logger());
app.use(xp::requestId());
app.use(xp::securityHeaders());
app.use(xp::bodyLimit(1024 * 1024));
app.use(xp::cors());
app.use(xp::rateLimit({.windowMs = 60000, .max = 120}));
```

Middleware runs in the order it is registered.

## Route-level middleware

```cpp
app.post("/api/echo", {
    xp::validate([](xp::Request& req) {
        return req.isJson() ? "" : "Expected application/json";
    })
}, handler);
```
