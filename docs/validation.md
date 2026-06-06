# Validation

`xp::validate()` runs a custom validation function before your route handler. If validation fails, a `400 Bad Request` is returned automatically.

## Basic usage

```cpp
app.post("/users",
    {xp::validate([](xp::Request& req) -> std::string {
        const auto body = req.json();
        if (!body.isMember("name") || body["name"].asString().empty()) {
            return "name is required";  // Return an error message to fail validation
        }
        if (!body.isMember("email") || body["email"].asString().empty()) {
            return "email is required";
        }
        return "";  // Return empty string to pass validation
    })},
    [](xp::Request& req, xp::Response& res) {
        // This only runs if validation passed
        const auto body = req.json();
        res.status(201).json({{"name", body["name"]}});
    }
);
```

The validator function receives the `Request` and should return:
- An **empty string** `""` to pass validation (continue to the handler)
- A **non-empty error message** to fail validation (sends `400 Bad Request`)

---

## Reusable validators

Define validators as named functions for reuse across routes:

```cpp
// Validator: ensure required JSON fields are present
auto requireFields(std::vector<std::string> fields) {
    return xp::validate([fields](xp::Request& req) -> std::string {
        const auto body = req.json();
        for (const auto& field : fields) {
            if (!body.isMember(field) || body[field].asString().empty()) {
                return "Field '" + field + "' is required.";
            }
        }
        return "";
    });
}

// Use it on multiple routes:
app.post("/users",   {requireFields({"name", "email"})},          handler);
app.post("/orders",  {requireFields({"productId", "quantity"})},  handler);
app.post("/reviews", {requireFields({"title", "body", "rating"})},handler);
```

---

## Combining multiple validators

```cpp
app.post("/register",
    {
        xp::bodyLimit(10 * 1024),  // Max 10KB body
        xp::validate([](xp::Request& req) -> std::string {
            if (!req.isJson()) return "Content-Type must be application/json";
            const auto body = req.json();
            if (!body.isMember("email")) return "email is required";
            if (!body.isMember("password")) return "password is required";
            if (body["password"].asString().size() < 8)
                return "password must be at least 8 characters";
            return "";
        })
    },
    [](xp::Request& req, xp::Response& res) {
        res.status(201).json({{"message", "Account created"}});
    }
);
```

---

## Error response

When validation fails:

```json
HTTP/1.1 400 Bad Request

{
  "status": "error",
  "message": "email is required"
}
```
