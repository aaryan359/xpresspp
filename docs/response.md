---
title: Response
description: Send JSON, text, HTML, redirects, files, and custom status codes.
---

# Response

`xp::Response` is chainable, so handlers can set the status and immediately send a response.

```cpp
res.status(201).json({
    {"status", "created"}
});

res.send("hello");
res.text("hello");
res.html("<h1>Welcome</h1>");
res.redirect("/login");
```

## JSON responses

```cpp
app.get("/api/ping", [](xp::Request& req, xp::Response& res) {
    res.json({
        {"status", "healthy"},
        {"framework", "Xpress++"}
    });
});
```

## Status codes

```cpp
res.status(404).json({
    {"status", "not_found"},
    {"message", "User not found"}
});
```

## Response shortcuts

```cpp
res.created({{"status", "created"}}, "/api/users/42");
res.noContent();
res.badRequest("Missing email");
res.unauthorized();
res.forbidden();
res.notFound();
res.serverError();
```

## Cookies

```cpp
res.cookie("session", token, 86400, true, true, "Lax");
res.clearCookie("session");
```
