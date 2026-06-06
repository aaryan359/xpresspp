# Error Handling

Xpress++ makes it easy to return clean, consistent errors from anywhere in your app.

## The simple way — throw an error

Throw a typed error from any handler or middleware. Xpress++ will catch it and automatically return the right HTTP response.

```cpp
#include <xpresspp/xpresspp.h>

app.get("/users/:id", [](xp::Request& req, xp::Response& res) {
    const auto id = req.param("id");

    if (id.empty()) {
        throw xp::BadRequestError("The 'id' parameter is required.");
    }

    auto user = db.find(id);
    if (!user) {
        throw xp::NotFoundError("User " + id + " not found.");
    }

    res.json(user->toJson());
});
```

No try/catch needed in the handler — Xpress++ handles it all.

---

## Typed error classes

Every error class maps to a specific HTTP status code.

| Class | Status | Include |
|-------|--------|---------|
| `xp::BadRequestError` | 400 | Bad input, missing fields |
| `xp::UnauthorizedError` | 401 | Not authenticated |
| `xp::ForbiddenError` | 403 | Not permitted |
| `xp::NotFoundError` | 404 | Resource doesn't exist |
| `xp::MethodNotAllowedError` | 405 | Wrong HTTP verb |
| `xp::ConflictError` | 409 | Duplicate resource |
| `xp::UnprocessableEntityError` | 422 | Validation failed |
| `xp::TooManyRequestsError` | 429 | Rate limited |
| `xp::InternalError` | 500 | Server failure |
| `xp::ServiceUnavailableError` | 503 | Temporary outage |

All errors accept an optional **hint** string shown in the JSON response when `debug = true`:

```cpp
throw xp::BadRequestError(
    "email is required",        // shown to the client (always)
    "Include 'email' in the JSON body: {\"email\": \"user@example.com\"}"  // shown in debug mode
);
```

---

## Convenience throw helpers

Instead of `throw xp::SomeError(...)`, you can use the free functions directly:

```cpp
xp::badRequest("email is required");
xp::unauthorized();
xp::forbidden("You are not an admin.");
xp::notFound("Post 42 not found.");
xp::conflict("A user with that email already exists.");
xp::unprocessable("Password must be at least 8 characters.");
xp::tooManyRequests();
xp::serverError("Database is unreachable.");
xp::abort(418, "I'm a teapot");  // any custom status
```

These are all `[[noreturn]]` — the compiler knows they never return.

---

## Generic HttpError

Use `xp::HttpError` for any status code that doesn't have a dedicated class:

```cpp
throw xp::HttpError(451, "Content unavailable for legal reasons.");
```

---

## What the response looks like

When you throw an error, Xpress++ returns a JSON response:

```json
{
  "status": "error",
  "message": "User 42 not found.",
  "path": "/users/42",
  "method": "GET",
  "hint": "Make sure the ID exists in the database."
}
```

- `hint` is only included when `debug = true` (the default).

---

## Global error handler

You can override the default behaviour for all errors using `app.onError()`:

```cpp
app.onError([](const std::exception& err, xp::Request& req, xp::Response& res) {
    // Check if it's an HTTP error with a status code
    int status = 500;
    if (const auto* he = dynamic_cast<const xp::HttpError*>(&err)) {
        status = he->statusCode();
    }

    res.status(status).json({
        {"success", false},
        {"error",   err.what()},
        {"path",    req.path()}
    });
});
```

::: warning Your error handler must always send a response
If your `onError` handler throws an exception itself, Xpress++ will catch it and send a fallback 500 response automatically.
:::

---

## Per-route error handling

You can also use a try/catch inside any handler for fine-grained control:

```cpp
app.get("/payment/:id", [](xp::Request& req, xp::Response& res) {
    try {
        const auto result = processPayment(req.param("id"));
        res.json(result);
    } catch (const PaymentDeclinedError& e) {
        res.status(402).json({{"error", "Payment declined"}, {"reason", e.what()}});
    } catch (const std::exception& e) {
        throw xp::InternalError("Unexpected payment error: " + std::string(e.what()));
    }
});
```

---

## Startup errors

If you misconfigure the app (bad port, missing TLS cert, etc.), Xpress++ will print a clear error and throw `xp::ConfigurationError` before the server starts.

```
  ✗ [xpress++ startup error]
  Invalid port: 99999

  → Fix: Ports must be in the range 1–65535.
```

These are framework-level errors — they always terminate the process with a helpful message.

---

## Debug vs production

In **debug mode** (default):
- Hints are shown in error responses
- 5xx errors are logged to `stderr` with colour
- The error type is visible in the response

In **production mode** (`app.debug(false)` or `config.debug = false`):
- Hints are hidden
- Minimal error info is exposed to the client

```cpp
xp::AppConfig config;
config.debug = false;  // set this for production!
app.configure(config);
```

---

## Standard C++ exceptions

Xpress++ also handles standard library exceptions gracefully:

| Exception | Status |
|-----------|--------|
| `std::invalid_argument` | 400 |
| `std::out_of_range` | 400 |
| `std::logic_error` | 500 |
| Any other `std::exception` | 500 |
| Non-`std::exception` throws | 500 |
