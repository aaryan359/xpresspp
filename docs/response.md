# Response

The `xp::Response` object lets you build and send the response back to the client.

## Sending text

```cpp
res.send("Hello, world!");
res.text("Same as send()");
```

## Sending HTML

```cpp
res.html("<h1>Welcome</h1><p>This is HTML.</p>");
```

## Sending JSON

The most common response type — automatically sets `Content-Type: application/json`.

### From an initializer list

```cpp
res.json({
    {"status", "ok"},
    {"userId", 42},
    {"name",   "Alice"}
});
```

### From a `Json::Value`

```cpp
Json::Value body;
body["users"][0]["name"] = "Alice";
body["users"][0]["email"] = "alice@example.com";
res.json(body);
```

### From a `std::unordered_map`

```cpp
std::unordered_map<std::string, std::string> data = {
    {"key1", "value1"},
    {"key2", "value2"}
};
res.json(data);
```

---

## HTTP status codes

Chain `.status()` before any response method:

```cpp
res.status(200).json({{"message", "OK"}});
res.status(201).json({{"id", 42}});
res.status(404).json({{"error", "Not found"}});
res.status(500).send("Internal error");
```

### Shorthand status methods

These set the status AND send the JSON response in one call:

```cpp
// 400 Bad Request
res.badRequest("email is required");

// 401 Unauthorized
res.unauthorized("Please log in");

// 403 Forbidden
res.forbidden("You cannot access this resource");

// 404 Not Found
res.notFound("User 42 not found");

// 500 Internal Server Error
res.serverError("Database connection failed");
```

### 201 Created with a Location header

```cpp
// Create a resource and tell the client where to find it:
res.created(
    {{"id", 42}, {"name", "Alice"}},
    "/users/42"   // Location header — optional
);
```

### 204 No Content

```cpp
res.noContent();  // Sends an empty 204 response
```

---

## Headers

```cpp
res.header("X-Custom-Header", "value");
res.header("Cache-Control",   "no-store");

// Content type shorthand:
res.type("text/plain; charset=utf-8");
```

---

## Redirects

```cpp
// 302 Temporary redirect (default):
res.redirect("/new-location");

// 301 Permanent redirect:
res.redirect("/new-location", 301);

// Full URL:
res.redirect("https://example.com");
```

Set just the Location header without sending (useful before other logic):

```cpp
res.location("/users/42");
```

---

## Cookies

```cpp
res.cookie(
    "session_id",   // cookie name
    "abc123",       // cookie value
    86400,          // max-age in seconds (0 = session cookie)
    true,           // HttpOnly (default: true)
    false,          // Secure (default: false)
    "Lax"           // SameSite (default: "Lax")
);

// Clear a cookie:
res.clearCookie("session_id");
```

---

## Sending files

Send a file as a download or inline:

```cpp
// Serve a file (browser shows it inline if possible):
res.file("/path/to/document.pdf");

// Force-download with a custom filename:
res.download("/path/to/report-2026.pdf", "annual-report.pdf");
```

MIME types are detected automatically from the file extension:

| Extension | MIME type |
|-----------|-----------|
| `.html` | `text/html; charset=utf-8` |
| `.css` | `text/css; charset=utf-8` |
| `.js` | `application/javascript` |
| `.json` | `application/json` |
| `.png` | `image/png` |
| `.jpg` | `image/jpeg` |
| `.svg` | `image/svg+xml` |
| `.pdf` | `application/pdf` |
| other | `application/octet-stream` |

---

## Method chaining

Most `Response` methods return `res&` so you can chain them:

```cpp
res.status(201)
   .header("X-Resource-Id", "42")
   .json({{"id", 42}, {"name", "Alice"}});
```

---

## Check if response was sent

Use `res.sent()` to check whether a response has already been sent (useful in middleware):

```cpp
if (!res.sent()) {
    res.json({{"error", "Something went wrong"}});
}
```

---

## Quick reference

| Method | Description |
|--------|-------------|
| `res.send(text)` | Send plain text |
| `res.text(text)` | Alias for `send()` |
| `res.html(markup)` | Send HTML |
| `res.json(value)` | Send JSON |
| `res.status(code)` | Set HTTP status code |
| `res.header(key, val)` | Set a response header |
| `res.type(mime)` | Set Content-Type |
| `res.redirect(url, code)` | Redirect to URL |
| `res.location(url)` | Set Location header |
| `res.cookie(...)` | Set a cookie |
| `res.clearCookie(name)` | Clear a cookie |
| `res.file(path)` | Serve a file |
| `res.download(path, name)` | Force-download a file |
| `res.noContent()` | Send 204 No Content |
| `res.created(json, url)` | Send 201 Created |
| `res.badRequest(msg)` | Send 400 |
| `res.unauthorized(msg)` | Send 401 |
| `res.forbidden(msg)` | Send 403 |
| `res.notFound(msg)` | Send 404 |
| `res.serverError(msg)` | Send 500 |
| `res.sent()` | Check if response sent |
