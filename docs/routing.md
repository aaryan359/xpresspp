---
title: Routing
description: Define HTTP routes, dynamic parameters, and wildcard handlers.
---

# Routing

Routes map HTTP methods and URL paths to handler functions.

## Basic route

```cpp
app.get("/api/ping", [](xp::Request& req, xp::Response& res) {
    res.json({
        {"status", "healthy"}
    });
});
```

## Supported methods

```cpp
app.get(path, handler);
app.post(path, handler);
app.put(path, handler);
app.patch(path, handler);
app.del(path, handler);
app.options(path, handler);
app.head(path, handler);
app.all(path, handler);
```

## Dynamic parameters

Use Express-style `:name` segments:

```cpp
app.get("/api/users/:id", [](xp::Request& req, xp::Response& res) {
    res.json({
        {"id", req.param("id")}
    });
});
```

## Optional parameters

Use `?` after a parameter name when the segment may be absent:

```cpp
app.get("/docs/:section?", [](xp::Request& req, xp::Response& res) {
    res.json({
        {"section", req.param("section")}
    });
});
```

## Trailing slash behavior

Routes are strict by default. Disable strict trailing slashes when `/users` and `/users/` should match the same handler:

```cpp
app.configure({
    .strictTrailingSlash = false
});
```

## Wildcards

Use `*` to capture the rest of a path:

```cpp
app.get("/files/*", [](xp::Request& req, xp::Response& res) {
    res.json({
        {"path", req.param("wildcard")}
    });
});
```

## Route groups

Group routes under a shared prefix without paying for runtime path rewriting:

```cpp
app.group("/api", [](xp::Router& api) {
    api.get("/users/:id", handler);
    api.post("/users", createUser);
});
```

## Route-level middleware

Pass middleware directly to a route when only that endpoint needs a guard:

```cpp
app.get("/admin", {
    xp::apiKeyAuth("secret")
}, [](xp::Request& req, xp::Response& res) {
    res.json({{"status", "ok"}});
});
```
