# Rate Limiting

Protect your API from abuse by limiting how many requests a single IP can make in a time window.

## Basic usage

```cpp
#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    // Allow max 100 requests per minute per IP (default settings)
    app.use(xp::rateLimit());

    app.get("/api/data", [](xp::Request& req, xp::Response& res) {
        res.json({{"data", "..."}});
    });

    app.listen(8080);
}
```

---

## Custom options

```cpp
xp::RateLimitOptions opts;
opts.windowMs = 15 * 60 * 1000;  // 15 minutes in milliseconds
opts.max      = 100;               // Max 100 requests per window
opts.message  = "You have sent too many requests. Please wait and try again.";

app.use(xp::rateLimit(opts));
```

---

## Per-route rate limiting

Apply tighter limits to specific routes (e.g. login endpoint):

```cpp
// Strict limit on the login endpoint to prevent brute force
app.post("/api/login",
    {xp::rateLimit({.windowMs = 60000, .max = 5})},  // 5 attempts per minute
    [](xp::Request& req, xp::Response& res) {
        // Login logic...
    }
);

// More relaxed limit for general API
app.use(xp::rateLimit({.windowMs = 60000, .max = 200}));
```

---

## Response when limit is exceeded

When the limit is hit, the client receives:

```
HTTP/1.1 429 Too Many Requests

{
  "status": "error",
  "message": "Too many requests. Please try again later."
}
```

---

## Options reference

| Field | Default | Description |
|-------|---------|-------------|
| `windowMs` | `60000` (1 minute) | Time window in milliseconds |
| `max` | `100` | Maximum requests allowed per window per IP |
| `message` | `"Too many requests..."` | Error message shown to the client |

::: info In-memory storage
The rate limiter uses in-memory storage per server process. If you run multiple server instances behind a load balancer, each instance tracks limits independently. For distributed rate limiting, use Redis or a shared cache.
:::
