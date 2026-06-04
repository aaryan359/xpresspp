---
title: Security
description: Add opt-in security middleware for production APIs.
---

# Security

Security middleware is opt-in and only runs when registered.

```cpp
app.use(xp::securityHeaders());
app.use(xp::bodyLimit(1024 * 1024));
app.use(xp::csrf());
```

## Security headers

```cpp
app.use(xp::securityHeaders({
    .hsts = true
}));
```

The middleware can set `X-Content-Type-Options`, `X-Frame-Options`, `Referrer-Policy`, `X-XSS-Protection`, and `Strict-Transport-Security`.

## CSRF

```cpp
app.use(xp::csrf({
    .cookie = "csrf_token",
    .header = "x-csrf-token"
}));
```

Safe methods such as `GET`, `HEAD`, and `OPTIONS` pass through automatically.
