# Logging

Xpress++ includes two built-in logging middleware to help you see what's happening in your server.

## `xp::logger()` — Development logging

Color-coded, human-friendly request/response logs for your terminal.

```cpp
app.use(xp::logger());
```

Output (in a color terminal):

```
    GET  /api/ping  200  3ms
   POST  /api/users  201  12ms
 DELETE  /api/users/42  404  1ms
```

**Color coding:**
- Methods: `GET`=green, `POST`=blue, `PUT`=yellow, `PATCH`=magenta, `DELETE`=red
- Status codes: 2xx=green, 3xx=cyan, 4xx=yellow, 5xx=red

Colors are automatically **disabled** when the output is not a terminal (e.g. log files, CI/CD pipelines).

---

## `xp::combinedLogger()` — Production logging

Apache/Nginx combined-log format — suitable for log aggregation tools (Loki, Datadog, Splunk, etc.):

```cpp
app.use(xp::combinedLogger());
```

Output:

```
[2026-06-06 12:34:56] GET /api/ping 200 3ms - "Mozilla/5.0 (Linux; ...)"
[2026-06-06 12:34:57] POST /api/users 201 12ms - "curl/7.88.1"
```

---

## Recommended middleware order

Put the logger early so it captures all request processing time:

```cpp
app.use(xp::requestId());    // 1. Attach unique ID
app.use(xp::logger());       // 2. Log all requests
app.use(xp::cors());         // 3. CORS headers
app.use(xp::rateLimit());    // 4. Rate limiting
// ... route handlers
```

---

## Writing your own logger

You can build a custom logger as middleware:

```cpp
auto myLogger = [](xp::Request& req, xp::Response& res, xp::Next next) {
    const auto start = std::chrono::steady_clock::now();
    next();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start
    ).count();

    // Log in JSON format for structured logging
    std::cout << "{"
        << "\"method\":\"" << req.method() << "\","
        << "\"path\":\""   << req.path()   << "\","
        << "\"status\":"   << res.statusCode() << ","
        << "\"ms\":"       << ms
        << "}\n";
};

app.use(myLogger);
```
