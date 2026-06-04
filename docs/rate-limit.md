---
title: Rate Limiting
description: Protect endpoints with the built-in request rate limiter.
---

# Rate Limiting

Rate limiting helps protect APIs from noisy clients and repeated bursts.

```cpp
app.use(xp::rateLimit({
    .windowMs = 60000,
    .max = 120,
    .message = "Too many requests."
}));
```

The middleware tracks clients by request IP and returns `429` when the limit is exceeded.
