# Security Headers

`xp::securityHeaders()` adds HTTP security headers to every response. These headers protect against common attacks like XSS, clickjacking, and MIME sniffing.

## Basic usage

```cpp
app.use(xp::securityHeaders());
```

That's it. By default it sets these headers on every response:

| Header | Value | Protects against |
|--------|-------|-----------------|
| `X-Content-Type-Options` | `nosniff` | MIME type sniffing |
| `X-Frame-Options` | `DENY` | Clickjacking |
| `Referrer-Policy` | `no-referrer` | Information leakage |
| `X-XSS-Protection` | `0` | Old XSS filter bugs |

---

## Custom options

```cpp
xp::SecurityHeadersOptions opts;
opts.contentTypeOptions  = true;
opts.frameOptions        = true;
opts.frameOptionsValue   = "SAMEORIGIN"; // Allow iframes from same origin
opts.referrerPolicy      = true;
opts.referrerPolicyValue = "strict-origin-when-cross-origin";
opts.xssProtection       = true;
opts.hsts                = true;  // Enable HSTS (HTTPS only!)
opts.hstsValue           = "max-age=31536000; includeSubDomains";

app.use(xp::securityHeaders(opts));
```

::: danger Enable HSTS only on HTTPS
`hsts` (HTTP Strict Transport Security) tells browsers to **always** use HTTPS for your domain. Only enable it when your server is running with a valid TLS certificate — it cannot be easily undone.
:::

---

## Options reference

| Field | Default | Description |
|-------|---------|-------------|
| `contentTypeOptions` | `true` | Set `X-Content-Type-Options: nosniff` |
| `frameOptions` | `true` | Set `X-Frame-Options` |
| `frameOptionsValue` | `"DENY"` | Value for `X-Frame-Options` |
| `referrerPolicy` | `true` | Set `Referrer-Policy` |
| `referrerPolicyValue` | `"no-referrer"` | Value for `Referrer-Policy` |
| `xssProtection` | `true` | Set `X-XSS-Protection: 0` |
| `hsts` | `false` | Set `Strict-Transport-Security` |
| `hstsValue` | `"max-age=15552000; includeSubDomains"` | Value for HSTS header |
