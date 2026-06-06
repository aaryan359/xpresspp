# Request ID

`xp::requestId()` attaches a unique ID to every request and echoes it back in the response header. This makes tracing a request through logs much easier.

## Basic usage

```cpp
app.use(xp::requestId());
```

Every response will now include an `X-Request-Id` header:

```
HTTP/1.1 200 OK
X-Request-Id: req-42
```

If the incoming request already has an `X-Request-Id` header, that value is used instead of generating a new one. This allows clients or proxies to inject their own trace IDs.

---

## Custom header name

```cpp
app.use(xp::requestId("X-Trace-Id"));
```

---

## Using the request ID in your handlers

```cpp
app.get("/api/data", [](xp::Request& req, xp::Response& res) {
    // Read the ID that was set by the middleware
    const auto id = req.header("x-request-id");
    std::cout << "[" << id << "] Processing /api/data\n";

    res.json({{"data", "..."}});
});
```

::: tip Combine with logger
Use `xp::requestId()` before `xp::logger()` so the request ID is available for structured log output.
:::
