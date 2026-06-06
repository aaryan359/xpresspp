# Xpress++

A C++ web framework that gives you Express-style simplicity on top of Drogon — one of the fastest HTTP servers ever benchmarked.

Drogon is extremely fast, but most developers avoid it because the API is complex and requires a lot of boilerplate. Xpress++ wraps it in a familiar, minimal interface so you can build a working server in minutes instead of hours.

---

## What it looks like

```cpp
#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    app.use(xp::logger());
    app.use(xp::cors());

    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.json({{"message", "Hello from Xpress++"}});
    });

    app.get("/users/:id", [](xp::Request& req, xp::Response& res) {
        const auto id = req.param("id");
        res.json({{"userId", id}, {"name", "Alice"}});
    });

    app.post("/users", [](xp::Request& req, xp::Response& res) {
        const auto body = req.json();
        if (!body.isMember("name")) {
            throw xp::BadRequestError("name is required");
        }
        res.status(201).json({{"created", body["name"]}});
    });

    app.listen(8080);
    return 0;
}
```

---

## Requirements

- GCC 11+ or Clang 13+ (C++20 support required)
- CMake 3.16+
- Drogon (latest)
- jsoncpp

### Install dependencies on Ubuntu

```bash
sudo apt install -y build-essential cmake libssl-dev libjsoncpp-dev zlib1g-dev

git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

### Install dependencies on macOS

```bash
brew install cmake jsoncpp openssl drogon
```

---

## Quick start

### 1. Clone the repo

```bash
git clone https://github.com/aaryan359/xpresspp
cd xpresspp
```

### 2. Build and install the CLI

```bash
cmake -S cli -B cli/build -DCMAKE_BUILD_TYPE=Release
cmake --build cli/build
sudo cp cli/build/xp /usr/local/bin/xp
```

### 3. Create your first app

```bash
xp create my-api
cd my-api
xp run
```

Your server is now running at `http://localhost:8080`.

### 4. Test it

```bash
curl http://localhost:8080/api/ping
```

---

## The CLI

The `xp` command handles all the CMake work for you.

| Command | What it does |
|---------|-------------|
| `xp create <name>` | Create a new project |
| `xp build` | Compile in debug mode |
| `xp build --release` | Compile with optimisations |
| `xp run` | Build and run the server |
| `xp run --release` | Build (release) and run |
| `xp watch` | Rebuild automatically on file changes |
| `xp clean` | Delete the build directory |
| `xp doctor` | Check all system dependencies |

Run `xp doctor` any time something is not working — it checks CMake, the compiler, Drogon, jsoncpp, OpenSSL, and the Xpress++ headers.

---

## Routing

```cpp
app.get("/path",    handler);   // GET
app.post("/path",   handler);   // POST
app.put("/path",    handler);   // PUT
app.patch("/path",  handler);   // PATCH
app.del("/path",    handler);   // DELETE  (del, not delete — delete is a C++ keyword)
app.all("/path",    handler);   // Any method

// URL parameters
app.get("/users/:id", [](xp::Request& req, xp::Response& res) {
    const auto id = req.param("id");
});

// Query string  (/search?q=hello)
app.get("/search", [](xp::Request& req, xp::Response& res) {
    const auto q = req.query("q");
});

// Route groups
app.group("/api/v1", [](xp::Router& r) {
    r.get("/users", handler);
    r.post("/users", handler);
});
```

---

## Request

```cpp
req.param("id")       // URL parameter
req.query("page")     // Query string value
req.body()            // Raw body string
req.json()            // Parsed JSON body (returns Json::Value)
req.form()            // Form fields (map)
req.header("key")     // Request header
req.cookie("key")     // Cookie value
req.ip()              // Client IP address
req.method()          // "GET", "POST", etc.
req.path()            // URL path
req.isJson()          // true if Content-Type is application/json
```

---

## Response

```cpp
res.send("text");                  // Plain text
res.html("<h1>Hello</h1>");        // HTML
res.json({{"key", "value"}});      // JSON
res.status(201).json({...});       // Status + JSON
res.redirect("/new-path");         // Redirect
res.file("/path/to/file.pdf");     // Serve a file
res.download("/path", "name.pdf"); // Force download
res.header("X-Key", "value");      // Set header
res.cookie("name", "value", 3600); // Set cookie
res.noContent();                   // 204 No Content

// Shorthand status helpers
res.badRequest("message");     // 400
res.unauthorized("message");   // 401
res.forbidden("message");      // 403
res.notFound("message");       // 404
res.serverError("message");    // 500
```

---

## Middleware

```cpp
// Global middleware (runs on every request)
app.use(xp::logger());
app.use(xp::cors());
app.use(xp::rateLimit());
app.use(xp::securityHeaders());

// Route-level middleware
app.get("/dashboard", {xp::apiKeyAuth("my-key")}, handler);

// Write your own
app.use([](xp::Request& req, xp::Response& res, xp::Next next) {
    // Do something before the handler
    next();
    // Do something after the handler
});
```

---

## Built-in middleware

| Middleware | What it does |
|------------|-------------|
| `xp::logger()` | Logs each request with method, path, status, and time |
| `xp::cors(options)` | Adds CORS headers |
| `xp::rateLimit(options)` | Limits requests per IP per time window |
| `xp::apiKeyAuth(key)` | Requires an API key in the `X-API-Key` header |
| `xp::basicAuth(fn)` | HTTP Basic authentication |
| `xp::securityHeaders(options)` | Sets `X-Frame-Options`, `Referrer-Policy`, and more |
| `xp::csrf(options)` | Verifies CSRF tokens |
| `xp::session(options)` | Cookie-based session IDs |
| `xp::requestId(header)` | Attaches a unique ID to each request |
| `xp::bodyLimit(bytes)` | Rejects bodies larger than the given size |
| `xp::validate(fn)` | Runs a custom validation function before the handler |

---

## Error handling

Throw a typed error from anywhere — Xpress++ catches it and sends the right HTTP response automatically.

```cpp
throw xp::BadRequestError("email is required");
throw xp::NotFoundError("User 42 not found");
throw xp::ForbiddenError("Admin only");
throw xp::ConflictError("Email already in use");
throw xp::InternalError("Database unreachable");

// Or use the throw helpers:
xp::notFound("Post 42 not found");
xp::abort(418, "I'm a teapot");
```

Available error types:

| Class | Status |
|-------|--------|
| `xp::BadRequestError` | 400 |
| `xp::UnauthorizedError` | 401 |
| `xp::ForbiddenError` | 403 |
| `xp::NotFoundError` | 404 |
| `xp::ConflictError` | 409 |
| `xp::UnprocessableEntityError` | 422 |
| `xp::TooManyRequestsError` | 429 |
| `xp::InternalError` | 500 |
| `xp::ServiceUnavailableError` | 503 |

In debug mode (the default), error responses include a `hint` field with actionable information. In production (`config.debug = false`), hints are hidden.

### Global error handler

```cpp
app.onError([](const std::exception& e, xp::Request& req, xp::Response& res) {
    int status = 500;
    if (const auto* he = dynamic_cast<const xp::HttpError*>(&e)) {
        status = he->statusCode();
    }
    res.status(status).json({{"error", e.what()}});
});
```

---

## Configuration

```cpp
xp::AppConfig config;
config.port               = 3000;
config.host               = "0.0.0.0";
config.debug              = false;   // disable in production
config.gzip               = true;
config.maxBodySize        = 5 * 1024 * 1024;  // 5 MB
config.threads            = 4;
config.caseSensitiveRouting = true;
config.strictTrailingSlash  = false;

app.configure(config);
app.listen();
```

Read from environment variables:

```cpp
config.port  = xp::Config::envInt("PORT",  8080);
config.debug = xp::Config::envBool("DEBUG", false);
```

---

## Project structure

A new project created with `xp create` looks like this:

```
my-app/
├── main.cpp
├── CMakeLists.txt
├── uploads/
└── vendor/
    └── xpresspp/
        └── include/
            └── xpresspp/
                ├── xpresspp.h    <- include this one file for everything
                ├── app.h
                ├── request.h
                ├── response.h
                ├── errors.h
                └── middleware/
```

You only ever need one include:

```cpp
#include <xpresspp/xpresspp.h>
```

---

## Documentation

Full documentation is in the `docs/` folder and can be run locally:

```bash
cd docs
npm install
npm run dev
```

Opens at `http://localhost:5173`.

---

## Repository layout

```
xpress++/
├── core/                  <- Framework headers (header-only)
│   └── include/xpresspp/
├── cli/                   <- The xp command-line tool (C++)
│   └── src/main.cpp
├── templates/             <- Project templates for xp create
│   └── default_app/
├── docs/                  <- VitePress documentation site
└── install.sh             <- One-line install script
```

---

## Contributing

1. Fork the repository
2. Create a branch: `git checkout -b feat/your-feature`
3. Make your changes
4. Test inside `templates/default_app` with `xp build`
5. Open a pull request

---

## License

MIT
