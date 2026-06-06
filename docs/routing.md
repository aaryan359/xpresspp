# Routing

Routing is how you tell Xpress++ which code to run when a request comes in.

## Basic routes

Every route is a combination of an **HTTP method**, a **path**, and a **handler function**.

```cpp
app.get("/", [](xp::Request& req, xp::Response& res) {
    res.send("Hello world!");
});

app.post("/users", [](xp::Request& req, xp::Response& res) {
    // Create a user
    res.status(201).json({{"message", "Created"}});
});
```

### Available methods

| Method | Description |
|--------|-------------|
| `app.get(path, handler)` | Read data |
| `app.post(path, handler)` | Create data |
| `app.put(path, handler)` | Replace data |
| `app.patch(path, handler)` | Partially update data |
| `app.del(path, handler)` | Delete data |
| `app.options(path, handler)` | CORS preflight etc. |
| `app.head(path, handler)` | Same as GET but no body |
| `app.all(path, handler)` | Match any HTTP method |

::: info Why `app.del` and not `app.delete`?
`delete` is a C++ keyword, so we use `del` instead.
:::

---

## URL parameters

Capture dynamic segments using `:name` in your path.

```cpp
app.get("/users/:id", [](xp::Request& req, xp::Response& res) {
    const auto id = req.param("id");
    res.json({{"userId", id}});
});

app.get("/posts/:year/:month", [](xp::Request& req, xp::Response& res) {
    const auto year  = req.param("year");
    const auto month = req.param("month");
    res.json({{"year", year}, {"month", month}});
});
```

Test:

```bash
curl /users/42       → {"userId":"42"}
curl /posts/2026/06  → {"year":"2026","month":"06"}
```

### Optional parameters

Add `?` to make a segment optional:

```cpp
app.get("/users/:id?", [](xp::Request& req, xp::Response& res) {
    const auto id = req.param("id");
    if (id.empty()) {
        res.json({{"message", "List all users"}});
    } else {
        res.json({{"userId", id}});
    }
});
```

### Wildcard routes

Use `*` to match everything after a prefix:

```cpp
app.get("/files/*", [](xp::Request& req, xp::Response& res) {
    const auto path = req.param("wildcard");
    res.send("You requested: " + path);
});
```

---

## Query parameters

Query parameters appear after `?` in the URL: `/search?q=hello&page=2`

```cpp
app.get("/search", [](xp::Request& req, xp::Response& res) {
    const auto q    = req.query("q");       // "hello"
    const auto page = req.query("page");    // "2"

    res.json({{"query", q}, {"page", page}});
});
```

Get all query params at once:

```cpp
const auto all = req.query();  // returns unordered_map<string, string>
```

---

## Route groups

Group related routes under a shared prefix using `app.group()`:

```cpp
app.group("/api/v1", [](xp::Router& r) {
    r.get("/users", [](xp::Request& req, xp::Response& res) {
        res.json({{"users", {}}});
    });

    r.post("/users", [](xp::Request& req, xp::Response& res) {
        res.status(201).json({{"message", "User created"}});
    });

    r.get("/users/:id", [](xp::Request& req, xp::Response& res) {
        res.json({{"id", req.param("id")}});
    });
});
```

All routes above will be available at `/api/v1/users`, `/api/v1/users/:id`, etc.

---

## Sub-routers

For large apps, split routes into separate `Router` objects:

```cpp
// In users_router.cpp (or inline):
xp::Router usersRouter;

usersRouter.get("/", [](xp::Request& req, xp::Response& res) {
    res.json({{"users", {}}});
});

usersRouter.get("/:id", [](xp::Request& req, xp::Response& res) {
    res.json({{"id", req.param("id")}});
});

// Mount it under /api/users:
app.use("/api/users", usersRouter);
```

---

## Route-level middleware

Apply middleware only to a specific route by passing it as the second argument:

```cpp
// Only this route will require authentication
app.get("/dashboard", {xp::apiKeyAuth("my-secret-key")}, [](xp::Request& req, xp::Response& res) {
    res.send("Welcome to your dashboard!");
});

// Multiple middleware for one route:
app.post("/admin/data",
    {xp::rateLimit({.max = 10}), xp::apiKeyAuth("admin-key")},
    [](xp::Request& req, xp::Response& res) {
        res.json({{"status", "ok"}});
    }
);
```

---

## Case sensitivity and trailing slashes

By default routes are **case-sensitive** and **trailing slashes matter**.

Change this in the config:

```cpp
xp::AppConfig config;
config.caseSensitiveRouting = false;  // /Users == /users
config.strictTrailingSlash  = false;  // /api/ == /api

app.configure(config);
```

---

## 404 — Not Found handler

Customise what happens when no route matches:

```cpp
app.notFound([](xp::Request& req, xp::Response& res) {
    res.status(404).json({
        {"error",   "Not found"},
        {"path",    req.path()},
        {"method",  req.method()}
    });
});
```

---

## 405 — Method Not Allowed handler

Called when the path matches but not the HTTP method:

```cpp
app.methodNotAllowed([](xp::Request& req, xp::Response& res) {
    res.status(405).json({
        {"error",   "Method not allowed"},
        {"method",  req.method()},
        {"path",    req.path()}
    });
});
```

---

## Listing all routes

Useful for debugging — print every registered route at startup:

```cpp
for (const auto& route : app.routes()) {
    std::cout << route.method << "  " << route.path << "\n";
}
```
