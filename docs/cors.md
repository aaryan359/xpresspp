---
title: CORS
description: Configure cross-origin request headers with Xpress++ middleware.
---

# CORS

Use the built-in CORS middleware for development defaults or explicit production settings.

```cpp
app.use(xp::cors());
```

## Options

```cpp
app.use(xp::cors({
    .origin = "https://example.com",
    .methods = "GET,POST,OPTIONS",
    .credentials = true
}));
```
