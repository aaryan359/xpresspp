# Alpha hardening and performance boundaries

Xpress++ keeps Drogon for sockets, HTTP parsing, TLS, the event loop, and coroutine
scheduling. Those components are security-sensitive and already optimized. Xpress++
owns the developer API, routing policy, middleware, validation, errors, caching,
database client generation, migrations, testing, and CLI workflows.

This separation avoids an additional network hop or serialization layer: request and
response wrappers hold Drogon objects directly, and the process-local `xp::xpc` cache
stores `xp::var` values without serialization.

## New alpha APIs

```cpp
xp::App app;

app.health();
app.ready({[] { return databaseIsReady(); }});

app.use(xp::responseTime());
app.use(xp::securityHeaders({
    .hsts = true,
    .contentSecurityPolicy = "default-src 'self'"
}));

app.onShutdown([] {
    // Flush application-owned resources.
});

xp::xpc.set("user:42", user, 60);
auto cached = xp::xpc.get("user:42");
```

Forwarding headers are ignored unless the application explicitly opts into a trusted
reverse proxy:

```cpp
xp::AppConfig config;
config.trustProxy = true;
app.configure(config);
```

Only enable this when the application is reachable exclusively through a proxy that
overwrites `X-Forwarded-For` and `X-Real-IP`.

Asynchronous middleware is registered with the same simple `app.use(...)` syntax. It
forms an asynchronous stage before the existing synchronous middleware stage; route
handlers remain unchanged.

## Release checks

Run the framework suite with:

```bash
xp test
```

Destructive migrations now require explicit acknowledgement:

```bash
xp migrate remove_legacy_fields --accept-data-loss
```

Each generated migration also stores the exact `schema.xp.snapshot` used to create it.
