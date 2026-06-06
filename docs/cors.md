# CORS

CORS (Cross-Origin Resource Sharing) is required when a browser on a different domain (e.g. your React frontend) calls your API.

## Basic usage

Add `xp::cors()` as global middleware to allow all origins:

```cpp
#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    app.use(xp::cors());  // Allow requests from any origin

    app.get("/api/data", [](xp::Request& req, xp::Response& res) {
        res.json({{"message", "Hello from the API"}});
    });

    app.listen(8080);
}
```

---

## Custom options

```cpp
xp::CorsOptions opts;
opts.origin      = "https://my-frontend.com"; // Only allow this origin
opts.methods     = "GET,POST,PUT,DELETE";      // Allowed HTTP methods
opts.headers     = "Content-Type,Authorization,X-API-Key"; // Allowed request headers
opts.credentials = true;  // Allow cookies/auth headers cross-origin

app.use(xp::cors(opts));
```

::: warning credentials + wildcard origin
If you set `credentials = true`, you **must** set a specific `origin` — browsers will reject `*` with credentials enabled.
:::

---

## Options reference

| Field | Default | Description |
|-------|---------|-------------|
| `origin` | `"*"` | Which origin(s) to allow |
| `methods` | `"GET,POST,PUT,PATCH,DELETE,OPTIONS,HEAD"` | Allowed HTTP methods |
| `headers` | `"Content-Type,Authorization"` | Allowed request headers |
| `credentials` | `false` | Allow cookies and auth headers |

---

## Preflight requests

Browsers send an `OPTIONS` request before any cross-origin request with custom headers or non-GET methods. `xp::cors()` handles this automatically — it responds to `OPTIONS` with `204 No Content`.

---

## CORS for specific routes only

If you only want CORS on certain routes, don't add it globally. Add it per route instead:

```cpp
app.get("/api/public", {xp::cors()}, [](xp::Request& req, xp::Response& res) {
    res.json({{"message", "Public endpoint"}});
});

// This route has no CORS headers:
app.get("/admin/data", [](xp::Request& req, xp::Response& res) {
    res.json({{"message", "Internal only"}});
});
```
