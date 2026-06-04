---
title: Configuration
description: Configure Xpress++ apps through AppConfig and environment helpers.
---

# Configuration

`xp::AppConfig` controls runtime settings without adding per-request overhead.

```cpp
xp::App app;

app.configure({
    .host = "0.0.0.0",
    .port = xp::Config::envInt("PORT", 8080),
    .debug = xp::Config::envBool("XP_DEBUG", true),
    .gzip = true,
    .sendfile = true,
    .maxBodySize = 1024 * 1024,
    .threads = 0
});

app.listen();
```

## Runtime options

| Option | Purpose |
| --- | --- |
| `host` | Default host used by `app.listen()`. |
| `port` | Default port used by `app.listen()`. |
| `debug` | Adds helpful development details to default error responses. |
| `caseSensitiveRouting` | Controls case sensitivity for route path matching. |
| `strictTrailingSlash` | Controls whether `/users` and `/users/` are different routes. |
| `gzip` | Enables Drogon gzip compression for eligible responses. |
| `sendfile` | Lets Drogon use sendfile for large static files. |
| `maxBodySize` | Sets Drogon's maximum accepted request body size. |
| `threads` | Sets Drogon's worker thread count when greater than zero. |

## Lifecycle hooks

```cpp
app.after([](xp::Request& req, xp::Response& res) {
    // Runs after the handler and error conversion finish.
});
```

## Shutdown

```cpp
xp::App::shutdown();
```

## TLS

```cpp
app.listenTls("0.0.0.0", 8443, "cert.pem", "key.pem");
```
