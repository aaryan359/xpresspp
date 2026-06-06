# CSRF Protection

CSRF (Cross-Site Request Forgery) is an attack where a malicious site tricks a user's browser into making a request to your server on their behalf. `xp::csrf()` defends against this by requiring a secret token on all state-changing requests.

## How it works

1. Your server sets a `csrf_token` cookie when the user visits.
2. Your frontend reads the cookie and sends the token back in an `X-CSRF-Token` header with every `POST`, `PUT`, `PATCH`, or `DELETE` request.
3. The middleware compares the cookie value to the header value. If they don't match, the request is rejected.

`GET`, `HEAD`, and `OPTIONS` requests are always allowed through (they shouldn't change state).

---

## Basic usage

```cpp
app.use(xp::csrf());
```

---

## Custom options

```cpp
xp::CsrfOptions opts;
opts.cookie = "my_csrf_token";     // Cookie name (default: "csrf_token")
opts.header = "x-custom-csrf";    // Header name (default: "x-csrf-token")

app.use(xp::csrf(opts));
```

---

## Frontend integration

**JavaScript (fetch API):**

```javascript
// Read the CSRF token from the cookie
function getCsrfToken() {
    return document.cookie
        .split('; ')
        .find(row => row.startsWith('csrf_token='))
        ?.split('=')[1];
}

// Include it in all mutating requests
fetch('/api/users', {
    method: 'POST',
    headers: {
        'Content-Type': 'application/json',
        'X-CSRF-Token': getCsrfToken()
    },
    body: JSON.stringify({name: 'Alice'})
});
```

---

## Error response

When the CSRF check fails:

```json
HTTP/1.1 403 Forbidden

{
  "status": "error",
  "message": "Invalid CSRF token."
}
```

---

## Options reference

| Field | Default | Description |
|-------|---------|-------------|
| `cookie` | `"csrf_token"` | Name of the cookie that stores the token |
| `header` | `"x-csrf-token"` | Name of the request header that carries the token |
