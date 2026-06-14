# Validation

Xpress++ includes an integrated validation middleware (`xp::validate`) to perform input validation before request handlers execute. If validation fails, it automatically rejects the request with a descriptive `400 Bad Request` error response.

You can validate request payloads in three ways:
1. **Fluent In-Route Validation** (Dot-chained, Zod-like builder schemas inside route handlers with custom error messages).
2. **Schema-Based Middleware** (Declarative validation before route execution).
3. **Custom Validation Logic** (Functional validation for dynamic/business rules).

---

## Fluent In-Route Validation (`req.validate`)

The most expressive way to validate request JSON payloads is using `req.validate(...)` directly inside your route handlers. This uses a Zod/Yup-like dot-chained builder pattern and supports C++17 structured bindings.

```cpp
app.post("/api/signup", [](xp::Request& req, xp::Response& res) {
    auto [body, err] = req.validate({
        {"username", xp::string().required("Username is required!").min(3, "Username must be 3+ characters")},
        {"password", xp::string().required().min(8)},
        {"email",    xp::string().required().email("Must be a valid email address")}
    });

    if (err) {
        res.status(400).json(err);
        return;
    }

    // body is safe to use as validated JSON!
    res.json({{"status", "success"}, {"username", body["username"]}});
});
```

### Fluent Chaining Methods

You can chain these rules on `xp::string()`, `xp::number()`, `xp::boolean()`, or `xp::any()`:

| Method | Description |
|--------|-------------|
| `.required(custom_msg)` | Enforces that the field exists and is not null. |
| `.min(length, custom_msg)` | String length must be at least `length` characters. |
| `.max(length, custom_msg)` | String length must be at most `length` characters. |
| `.email(custom_msg)` | String must match email structure. |
| `.custom(predicate, custom_msg)` | Pass a custom lambda `bool(const Json::Value&)` returning true on success. |
| `.unique(table, column, custom_msg)` | Database uniqueness check (Future integration). |

---

## Schema-Based Validation (Zod-like)

To perform schema-based validation, pass an initializer list of `xp::FieldRule` structures to `xp::validate`. The middleware automatically:
* Enforces `Content-Type: application/json`.
* Validates field presence (or honors optional fields).
* Enforces data types.
* Sends structured errors on failure.

```cpp
app.post("/api/register",
    xp::validate({
        {"username", xp::Type::String},              // Required string
        {"email",    xp::Type::String},              // Required string
        {"age",      xp::Type::Integer, false},      // Optional integer (third arg default is true)
        {"isAdmin",  xp::Type::Boolean, false}       // Optional boolean
    }),
    [](xp::Request& req, xp::Response& res) {
        // Safe to use: body is fully validated!
        auto body = req.json();
        res.json({{"success", true}});
    }
);
```

### Supported Data Types (`xp::Type`)

| Enum Value | Description |
|------------|-------------|
| `xp::Type::String` | Enforces JSON String |
| `xp::Type::Number` | Enforces JSON Numeric (Float or Integer) |
| `xp::Type::Integer`| Enforces JSON Integer |
| `xp::Type::Boolean`| Enforces JSON Boolean (`true` / `false`) |
| `xp::Type::Object` | Enforces JSON Object |
| `xp::Type::Array`  | Enforces JSON Array |

---

## Custom Functional Validation

For more complex constraints (like string length, email patterns, range limits, or database checks), pass a custom validator function to `xp::validate`.

The function receives the `Request` object and must return:
- An **empty string** `""` if validation passes.
- A **non-empty error message** string to fail validation.

```cpp
app.post("/api/posts",
    xp::validate([](xp::Request& req) -> std::string {
        auto body = req.json();
        
        if (!body.isMember("title") || body["title"].asString().empty()) {
            return "Title is required";
        }
        if (body["title"].asString().length() > 100) {
            return "Title must not exceed 100 characters";
        }
        if (body.isMember("rating") && (body["rating"].asInt() < 1 || body["rating"].asInt() > 5)) {
            return "Rating must be between 1 and 5";
        }
        
        return ""; // Success
    }),
    [](xp::Request& req, xp::Response& res) {
        res.status(201).json({{"message", "Post created!"}});
    }
);
```

---

## Reusable Validation Middleware

You can define and reuse validator logic across multiple routes by wrapping them in custom helper functions:

```cpp
// Validator generator function
auto requireFields(std::initializer_list<std::string> fields) {
    return xp::validate([fields = std::vector<std::string>(fields)](xp::Request& req) -> std::string {
        const auto body = req.json();
        for (const auto& field : fields) {
            if (!body.isMember(field) || body[field].asString().empty()) {
                return "Field '" + field + "' is required.";
            }
        }
        return "";
    });
}

// Reuse it on different endpoints:
app.post("/api/users",   requireFields({"name", "email"}), handler);
app.post("/api/reviews", requireFields({"title", "rating"}), handler);
```

---

## Error Response Format

When validation fails, Xpress++ immediately responds with:

```json
HTTP/1.1 400 Bad Request
Content-Type: application/json

{
  "status": "error",
  "message": "Field 'email' must be a string"
}
```
