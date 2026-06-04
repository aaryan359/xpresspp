---
title: Error Handling
description: Convert exceptions and missing routes into consistent HTTP responses.
---

# Error Handling

Xpress++ catches C++ exceptions at the request boundary and converts them into understandable HTTP responses.

```cpp
app.get("/api/error", [](xp::Request& req, xp::Response& res) {
    throw xp::badRequest("Missing required field: email");
});
```

## Custom error handler

```cpp
app.onError([](const std::exception& error, xp::Request& req, xp::Response& res) {
    res.status(500).json({
        {"status", "error"},
        {"message", error.what()}
    });
});
```

## Not found handler

```cpp
app.notFound([](xp::Request& req, xp::Response& res) {
    res.status(404).json({
        {"status", "not_found"},
        {"path", req.path()}
    });
});
```
