---
title: Authentication
description: Protect routes with API keys, basic auth, and custom guards.
---

# Authentication

Xpress++ includes small guard middleware for common API cases.

## API key

```cpp
app.get("/admin", {
    xp::apiKeyAuth("secret")
}, [](xp::Request& req, xp::Response& res) {
    res.json({{"status", "ok"}});
});
```

## Custom API key verification

```cpp
app.use(xp::apiKeyAuth([](const std::string& key) {
    return key == xp::Config::env("API_KEY");
}));
```

## Basic auth

```cpp
app.use(xp::basicAuth([](const std::string& user, const std::string& password) {
    return user == "admin" && password == xp::Config::env("ADMIN_PASSWORD");
}));
```

## Role guard

```cpp
app.use(xp::requireRole([](xp::Request& req) {
    return req.header("x-role") == "admin";
}));
```
