# Authentication

Xpress++ includes two ready-made authentication middleware: **API key** and **HTTP Basic Auth**.

## API Key authentication

Require clients to send a secret key in a request header.

### Simple static key

```cpp
// Clients must send: X-API-Key: my-secret-key
app.use(xp::apiKeyAuth("my-secret-key"));
```

Or use a custom header name:

```cpp
// Clients must send: Authorization: my-secret-key
app.use(xp::apiKeyAuth("my-secret-key", "authorization"));
```

### Dynamic key verification

For key rotation, database lookups, or multiple valid keys:

```cpp
app.use(xp::apiKeyAuth([](const std::string& key) -> bool {
    // Return true if the key is valid
    return validKeys.count(key) > 0;
}));
```

---

## HTTP Basic authentication

```cpp
// Clients send: Authorization: Basic base64(username:password)
app.use(xp::basicAuth([](const std::string& username, const std::string& password) -> bool {
    return username == "admin" && password == "secret";
}));
```

The middleware automatically:
- Decodes the Base64 `Authorization: Basic ...` header
- Calls your verifier with the plain-text username and password
- Returns `401 Unauthorized` with a `WWW-Authenticate: Basic` header if the credentials are wrong

---

## Role-based access

Use `xp::requireRole()` to allow access only when a custom predicate passes.

This is flexible — you can check any property on the request:

```cpp
// Only allow requests that have a "role" param set to "admin"
app.use("/admin", xp::requireRole([](xp::Request& req) -> bool {
    return req.param("role") == "admin";
}, "Admin access required."));
```

A realistic example using a token decoded in earlier middleware:

```cpp
// Middleware to decode JWT and set req params:
auto jwtMiddleware = [](xp::Request& req, xp::Response& res, xp::Next next) {
    const auto token = req.header("authorization");
    // Decode JWT and extract claims...
    req.setParam("userId", "42");
    req.setParam("role",   "admin");
    next();
};

app.use(jwtMiddleware);

// Now protect admin routes:
app.get("/admin/stats",
    {xp::requireRole([](xp::Request& req) { return req.param("role") == "admin"; })},
    [](xp::Request& req, xp::Response& res) {
        res.json({{"stats", "..."}});
    }
);
```

---

## Protecting specific routes

Don't add auth globally if only some routes need it — use route-level middleware:

```cpp
// Public routes:
app.get("/api/ping",  [](xp::Request& req, xp::Response& res) { res.send("pong"); });
app.post("/api/login",[](xp::Request& req, xp::Response& res) { /* ... */ });

// Protected routes — only these require an API key:
app.get("/api/users",
    {xp::apiKeyAuth("secret-key")},
    [](xp::Request& req, xp::Response& res) {
        res.json({{"users", {}}});
    }
);
```

---

## Error response

When authentication fails, the middleware sends:

```json
{
  "status": "error",
  "message": "Invalid API key."
}
```

For Basic Auth failures, it also sets the `WWW-Authenticate: Basic` header, which makes browsers show a login dialog.

---

## Quick reference

| Function | Description |
|----------|-------------|
| `xp::apiKeyAuth(key)` | Static key from `X-API-Key` header |
| `xp::apiKeyAuth(key, header)` | Static key from custom header |
| `xp::apiKeyAuth(verifierFn)` | Dynamic key with custom logic |
| `xp::basicAuth(verifierFn)` | HTTP Basic with username+password |
| `xp::requireRole(predicate)` | Allow only when predicate is true |
