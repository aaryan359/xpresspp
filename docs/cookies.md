# Cookies

Read cookies from requests and set cookies on responses.

## Reading cookies

```cpp
app.get("/profile", [](xp::Request& req, xp::Response& res) {
    // Read a single cookie
    const auto token  = req.cookie("session_token");
    const auto userId = req.cookie("user_id");

    if (token.empty()) {
        throw xp::UnauthorizedError("Please log in.");
    }

    res.json({{"userId", userId}});
});
```

Read all cookies at once:

```cpp
const auto cookies = req.cookies();  // unordered_map<string, string>
for (const auto& [name, value] : cookies) {
    std::cout << name << " = " << value << "\n";
}
```

---

## Setting cookies

```cpp
app.post("/login", [](xp::Request& req, xp::Response& res) {
    // Set a session cookie (expires when browser closes)
    res.cookie("session_id", "abc123");

    // Set a persistent cookie (expires in 7 days)
    res.cookie(
        "user_pref",     // name
        "dark-mode",     // value
        7 * 24 * 3600,   // max-age in seconds (7 days)
        true,            // HttpOnly
        false,           // Secure
        "Lax"            // SameSite
    );

    res.json({{"message", "Logged in"}});
});
```

### Cookie parameter reference

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `key` | `string` | required | Cookie name |
| `value` | `string` | required | Cookie value |
| `max_age_seconds` | `int` | `0` | `0` = session cookie; positive value = persistent |
| `http_only` | `bool` | `true` | Prevent JavaScript from reading the cookie |
| `secure` | `bool` | `false` | Only send over HTTPS |
| `same_site` | `string` | `"Lax"` | CSRF protection: `"Strict"`, `"Lax"`, or `"None"` |

---

## Clearing cookies

```cpp
app.post("/logout", [](xp::Request& req, xp::Response& res) {
    res.clearCookie("session_id");
    res.clearCookie("user_pref");
    res.json({{"message", "Logged out"}});
});
```

---

## Security recommendations

| Goal | Setting |
|------|---------|
| Prevent XSS from stealing cookie | `http_only = true` |
| Prevent sending cookie over HTTP | `secure = true` (HTTPS only) |
| Prevent CSRF | `same_site = "Strict"` or `"Lax"` |
| Short-lived session | Small `max_age_seconds` |

::: tip Best practice
Always set `HttpOnly = true` for session and auth cookies. This prevents JavaScript from stealing them via XSS attacks.
:::
