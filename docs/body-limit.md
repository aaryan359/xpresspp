# Body Limit

`xp::bodyLimit()` rejects requests whose body is larger than a specified size. This protects your server from memory exhaustion caused by oversized payloads.

## Basic usage

```cpp
// Reject bodies larger than 100 KB
app.use(xp::bodyLimit(100 * 1024));
```

---

## Per-route limits

Apply different limits for different routes:

```cpp
// Global limit: 50 KB
app.use(xp::bodyLimit(50 * 1024));

// File upload route gets a higher limit (10 MB)
app.post("/upload",
    {xp::bodyLimit(10 * 1024 * 1024)},
    [](xp::Request& req, xp::Response& res) {
        // Handle file upload
    }
);
```

---

## Error response

When the body is too large, the client receives:

```
HTTP/1.1 413 Payload Too Large

{
  "status": "error",
  "message": "Request body is too large."
}
```

---

## Size reference

| Size | Bytes |
|------|-------|
| 1 KB | `1024` |
| 10 KB | `10 * 1024` |
| 100 KB | `100 * 1024` |
| 1 MB | `1024 * 1024` |
| 10 MB | `10 * 1024 * 1024` |

::: tip App-level limit
You can also set a global body size limit in `AppConfig`:

```cpp
xp::AppConfig config;
config.maxBodySize = 5 * 1024 * 1024;  // 5 MB
app.configure(config);
```

This is enforced by Drogon before the body even reaches your handler.
:::
