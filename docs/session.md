# Session

`xp::session()` adds cookie-based session tracking to your app. Each visitor gets a unique session ID stored in a cookie.

## Basic usage

```cpp
app.use(xp::session());
```

Every request will now have a `xpresspp_session` cookie. New sessions are created automatically when a user visits for the first time or their session expires.

---

## Custom options

```cpp
xp::SessionOptions opts;
opts.cookie      = "my_session";  // Cookie name (default: "xpresspp_session")
opts.maxAgeSeconds = 3600;        // Expire after 1 hour (default: 86400 = 24 hours)
opts.secure      = true;          // Only send over HTTPS (default: false)

app.use(xp::session(opts));
```

---

## Reading the session ID

The session ID is stored in the cookie, so you read it like any other cookie:

```cpp
app.get("/profile", [](xp::Request& req, xp::Response& res) {
    const auto sessionId = req.cookie("xpresspp_session");
    res.json({{"sessionId", sessionId}});
});
```

::: info Session storage
The built-in session middleware only manages the session **ID** (a cookie). It does not store session data on the server. To store data associated with a session ID, use an in-memory map, Redis, or a database.
:::

---

## Storing session data (example pattern)

```cpp
#include <unordered_map>
#include <mutex>

// Simple in-memory session store
std::unordered_map<std::string, std::unordered_map<std::string, std::string>> sessionStore;
std::mutex sessionMutex;

// Middleware to attach session data to request
auto sessionData = [](xp::Request& req, xp::Response& res, xp::Next next) {
    const auto id = req.cookie("xpresspp_session");
    {
        std::lock_guard<std::mutex> lock(sessionMutex);
        if (!id.empty() && sessionStore.count(id)) {
            for (const auto& [k, v] : sessionStore[id]) {
                req.setParam("session_" + k, v);
            }
        }
    }
    next();
};

app.use(xp::session());
app.use(sessionData);
```

---

## Options reference

| Field | Default | Description |
|-------|---------|-------------|
| `cookie` | `"xpresspp_session"` | Cookie name |
| `maxAgeSeconds` | `86400` (24 hours) | Session lifetime in seconds |
| `secure` | `false` | Send cookie only over HTTPS |
