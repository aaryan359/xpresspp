# Authentication

Xpress++ includes built-in authentication middleware for **JWT (JSON Web Tokens)**, **API Key**, and **HTTP Basic Auth**.

---

## JWT Authentication (JSON Web Tokens)

Xpress++ provides ready-to-use functions for generating, verifying, and enforcing JWT authentication in your C++ routes using a simple, Express-like syntax.

### 1. Enforcing JWT on Routes (`xp::jwtAuth`)

Use `xp::jwtAuth(secret)` as route-level middleware. It automatically:
- Checks the `Authorization: Bearer <token>` header.
- Decodes and verifies the signature using HMAC-SHA256.
- Injects the authenticated `username` parameter into the request (available via `req.param("username")`).
- Sends a `401 Unauthorized` response with JSON if the token is invalid or missing.

```cpp
const string JWT_SECRET = "my-secret-key";

// Protect route with JWT middleware
app.get("/api/dashboard", xp::jwtAuth(JWT_SECRET), [](xp::Request& req, xp::Response& res) {
    auto username = req.param("username");
    res.json({
        {"success", true},
        {"message", "Welcome to your dashboard, " + username}
    });
});
```

### 2. Generating Tokens (`xp::generateJwt`)

To generate a token (typically during a login request):

```cpp
auto token = xp::generateJwt("aaryan", JWT_SECRET);
```

### 3. Verifying Tokens manually (`xp::verifyJwt`)

If you need to manually verify or decode a token:

```cpp
string username;
if (xp::verifyJwt(token, JWT_SECRET, username)) {
    // Token is valid! username now contains the claim
}
```

---

## Complete E2E Auth Flow (with async/await & database)

Here is how you can build a complete end-to-end user registration and authentication flow using the framework's intuitive `async` / `await` and database integrations:

```cpp
#include <xpresspp/xpresspp.h>
using namespace std;

const string JWT_SECRET = "your-app-jwt-secret";

int main() {
    xp::App app;

    // Database Setup
    xp::DbConfig db_config;
    db_config.driver = "postgresql";
    db_config.host = "127.0.0.1";
    db_config.port = 5450;
    db_config.database = "my_app";
    app.database(db_config);

    // Initialize users table asynchronously on startup
    app.onStart([]() async {
        await xp::query(
            "CREATE TABLE IF NOT EXISTS users ("
            "id SERIAL PRIMARY KEY, "
            "username VARCHAR(50) UNIQUE NOT NULL, "
            "password VARCHAR(50) NOT NULL"
            ");"
        );
    });

    // POST /api/auth/signup - Register a user
    app.post("/api/auth/signup", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            auto username = body["username"].asString();
            auto password = body["password"].asString();
            
            await xp::query(
                "INSERT INTO users (username, password) VALUES ($1, $2);",
                username, password
            );
            
            res.json({{"success", true}, {"message", "Registered successfully"}});
        } catch (...) {
            res.status(400).json({{"error", "Registration failed"}});
        }
    });

    // POST /api/auth/login - Return JWT Token
    app.post("/api/auth/login", [](xp::Request& req, xp::Response& res) async {
        try {
            auto body = req.json();
            auto username = body["username"].asString();
            auto password = body["password"].asString();
            
            auto result = await xp::query(
                "SELECT password FROM users WHERE username = $1;",
                username
            );
            
            if (result.empty() || result[0]["password"].as<string>() != password) {
                res.status(401).json({{"error", "Invalid credentials"}});
                co_return;
            }
            
            auto token = xp::generateJwt(username, JWT_SECRET);
            res.json({{"success", true}, {"token", token}});
        } catch (const exception& e) {
            res.status(500).json({{"error", e.what()}});
        }
    });

    // GET /api/secure-profile - Protected route
    app.get("/api/secure-profile", xp::jwtAuth(JWT_SECRET), [](xp::Request& req, xp::Response& res) {
        auto username = req.param("username");
        res.json({{"success", true}, {"user", username}});
    });

    app.listen(8082);
    return 0;
}
```

---

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

---

## Quick Reference

| Function | Description |
|----------|-------------|
| `xp::jwtAuth(secret)` | Built-in JWT verification middleware |
| `xp::generateJwt(username, secret)` | Generates signed HS256 JWT |
| `xp::verifyJwt(token, secret, username_out)` | Decodes and verifies JWT |
| `xp::apiKeyAuth(key)` | Static key from `X-API-Key` header |
| `xp::apiKeyAuth(verifierFn)` | Dynamic key with custom logic |
| `xp::basicAuth(verifierFn)` | HTTP Basic with username+password |
| `xp::requireRole(predicate)` | Allow only when predicate is true |
