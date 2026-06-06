# Request

The `xp::Request` object is passed to every handler. It gives you access to everything the client sent.

## Path & URL

```cpp
req.path()        // "/api/users/42"
req.url()         // same as path()
req.originalUrl() // same as path()
req.method()      // "GET", "POST", "DELETE", etc.
```

---

## URL Parameters (`:name`)

Captured from dynamic segments in the route path.

```cpp
// Route: /users/:id/posts/:postId
app.get("/users/:id/posts/:postId", [](xp::Request& req, xp::Response& res) {
    const auto userId = req.param("id");      // e.g. "42"
    const auto postId = req.param("postId");  // e.g. "7"

    res.json({{"userId", userId}, {"postId", postId}});
});
```

Get all params at once:

```cpp
const auto& params = req.params();  // const unordered_map<string, string>&
```

::: tip Missing params
`req.param("missing")` returns an empty string `""` — it never throws.
:::

---

## Query String

From a URL like `/search?q=hello&page=2`:

```cpp
const auto q    = req.query("q");      // "hello"
const auto page = req.query("page");   // "2"

// All query params:
const auto all = req.query();  // unordered_map<string, string>
```

---

## Request Body

### Raw body

```cpp
const std::string raw = req.body();
```

### JSON body

Use this when the client sends `Content-Type: application/json`.

```cpp
app.post("/users", [](xp::Request& req, xp::Response& res) {
    const auto data = req.json();   // returns Json::Value

    const auto name  = data["name"].asString();
    const auto email = data["email"].asString();
    const auto age   = data["age"].asInt();

    res.status(201).json({{"name", name}, {"email", email}});
});
```

::: warning Invalid JSON
If the body is not valid JSON, `req.json()` throws `std::invalid_argument`.
This is automatically caught and turned into a `400 Bad Request` response.
:::

### Form body (`application/x-www-form-urlencoded`)

```cpp
const auto fields = req.form();  // unordered_map<string, string>

const auto username = fields["username"];
const auto password = fields["password"];
```

### Multipart form / file uploads

```cpp
const auto parser = req.multipart();  // drogon::MultiPartParser

// All files:
const auto files = req.files();  // vector<drogon::HttpFile>

// A specific file by field name:
const auto avatar = req.file("avatar");  // optional<drogon::HttpFile>
if (avatar) {
    avatar->saveAs("uploads/" + avatar->getFileName());
}
```

---

## Headers

```cpp
req.header("content-type")    // "application/json"
req.header("authorization")   // "Bearer eyJ..."
req.header("x-custom-header") // any header, lowercased

// All headers:
const auto all = req.headers();  // unordered_map<string, string>
```

---

## Cookies

```cpp
const auto token    = req.cookie("session_token");
const auto userId   = req.cookie("user_id");

// All cookies:
const auto all = req.cookies();  // unordered_map<string, string>
```

---

## Content-Type helpers

```cpp
req.contentType()              // "application/json; charset=utf-8"
req.isJson()                   // true if Content-Type contains "application/json"
req.isForm()                   // true if Content-Type is form-urlencoded or multipart

req.is("text/html")            // true if Content-Type contains "text/html"
req.accepts("application/json") // true if Accept header allows JSON
```

---

## Client IP address

```cpp
req.ip()   // "192.168.1.1"  (direct connection IP)
req.ips()  // vector of all IPs from X-Forwarded-For + direct IP
```

---

## User Agent

```cpp
req.userAgent()  // "Mozilla/5.0 (Linux; ...)"
```

---

## Native Drogon request

If you need access to Drogon APIs directly:

```cpp
const auto& native = req.native();  // drogon::HttpRequestPtr
```

---

## Quick reference

| Method | Returns | Description |
|--------|---------|-------------|
| `req.path()` | `string` | URL path |
| `req.method()` | `string` | HTTP method |
| `req.param(key)` | `string` | URL parameter |
| `req.params()` | `map` | All URL parameters |
| `req.query(key)` | `string` | Query string value |
| `req.query()` | `map` | All query params |
| `req.body()` | `string` | Raw request body |
| `req.json()` | `Json::Value` | Parsed JSON body |
| `req.form()` | `map` | Form fields |
| `req.files()` | `vector` | Uploaded files |
| `req.file(key)` | `optional` | Single uploaded file |
| `req.header(key)` | `string` | Request header |
| `req.headers()` | `map` | All headers |
| `req.cookie(key)` | `string` | Cookie value |
| `req.cookies()` | `map` | All cookies |
| `req.ip()` | `string` | Client IP |
| `req.ips()` | `vector` | All client IPs |
| `req.userAgent()` | `string` | User-Agent header |
| `req.isJson()` | `bool` | Content-Type is JSON |
| `req.isForm()` | `bool` | Content-Type is form |
