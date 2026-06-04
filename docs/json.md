---
title: JSON
description: Parse request bodies and send structured JSON responses.
---

# JSON

Xpress++ uses Jsoncpp through compact request and response helpers.

```cpp
auto body = req.json();
auto name = body["name"].asString();

res.json({
    {"name", name},
    {"ok", true}
});
```

## Echo endpoint

```cpp
app.post("/api/echo", [](xp::Request& req, xp::Response& res) {
    const auto body = req.json();
    res.status(201).json({
        {"received", body}
    });
});
```
