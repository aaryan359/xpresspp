---
title: Request
description: Read request metadata, route parameters, query values, headers, and JSON bodies.
---

# Request

`xp::Request` wraps the incoming HTTP request with helpers that are intentionally close to Express.

| Helper | Description |
| --- | --- |
| `req.path()` | Current URL path. |
| `req.method()` | HTTP method string. |
| `req.param("id")` | Dynamic route parameter. |
| `req.query("page")` | Query-string value. |
| `req.query()` | All query-string values. |
| `req.json()` | Parsed JSON body. |
| `req.form()` | Parsed URL-encoded form body. |
| `req.files()` | Multipart uploaded files. |
| `req.header("Authorization")` | Header value. |
| `req.headers()` | All request headers. |
| `req.cookies()` | All cookies. |
| `req.accepts("application/json")` | Content negotiation helper. |

## Route parameters

```cpp
app.get("/api/users/:id", [](xp::Request& req, xp::Response& res) {
    res.json({
        {"id", req.param("id")},
        {"page", req.query("page")}
    });
});
```

## Content checks

```cpp
if (!req.isJson()) {
    res.badRequest("Expected application/json");
    return;
}
```

## JSON body

```cpp
app.post("/api/echo", [](xp::Request& req, xp::Response& res) {
    const auto body = req.json();

    res.status(201).json({
        {"received", body}
    });
});
```
