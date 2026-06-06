# Configuration

`AppConfig` lets you customise the framework's behaviour before calling `app.listen()`.

## Usage

```cpp
xp::AppConfig config;
config.host   = "0.0.0.0";
config.port   = 3000;
config.debug  = false;   // Disable debug mode in production
config.threads = 4;      // Use 4 worker threads

app.configure(config);
app.listen();            // Uses config.host and config.port
```

---

## All options

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `host` | `string` | `"0.0.0.0"` | Network interface to listen on |
| `port` | `int` | `8080` | TCP port to listen on |
| `debug` | `bool` | `true` | Show hints in error responses and log 5xx to stderr |
| `caseSensitiveRouting` | `bool` | `true` | `/Users` and `/users` are different routes |
| `strictTrailingSlash` | `bool` | `true` | `/api/` and `/api` are different routes |
| `gzip` | `bool` | `false` | Enable gzip compression for responses |
| `sendfile` | `bool` | `true` | Use OS sendfile() for faster file serving |
| `showBanner` | `bool` | `true` | Print the startup banner when server starts |
| `maxBodySize` | `size_t` | `1048576` (1 MB) | Maximum allowed request body size |
| `threads` | `size_t` | `0` (auto) | Number of worker threads (`0` = use Drogon default) |

---

## Production configuration example

```cpp
xp::AppConfig config;
config.host               = "0.0.0.0";
config.port               = 8080;
config.debug              = false;     // No hints exposed in errors
config.gzip               = true;      // Compress responses
config.maxBodySize        = 5 * 1024 * 1024;  // 5 MB
config.threads            = 8;         // 8 worker threads
config.caseSensitiveRouting = true;
config.strictTrailingSlash  = false;   // /api/ == /api

app.configure(config);
app.listen();
```

---

## Reading environment variables

Use `xp::Config` to read configuration from environment variables at startup:

```cpp
#include <xpresspp/xpresspp.h>

int main() {
    xp::AppConfig config;
    config.port  = xp::Config::envInt("PORT",  8080);
    config.debug = xp::Config::envBool("DEBUG", true);

    app.configure(config);
    app.listen();
}
```

### Config helper methods

| Method | Description | Example |
|--------|-------------|---------|
| `Config::env(key, fallback)` | Read a string env var | `Config::env("DB_URL", "localhost")` |
| `Config::envInt(key, fallback)` | Read an int env var | `Config::envInt("PORT", 8080)` |
| `Config::envBool(key, fallback)` | Read a bool env var (`"true"`, `"1"`, `"yes"`) | `Config::envBool("DEBUG", false)` |

```bash
# Set at launch:
PORT=3000 DEBUG=false ./my-app
```

---

## TLS / HTTPS

```cpp
app.listenTls(
    "0.0.0.0",
    443,
    "/etc/ssl/certs/my-cert.pem",
    "/etc/ssl/private/my-key.pem"
);
```

Xpress++ checks that the certificate and key files exist before starting. If they're missing, it prints a helpful error with an `openssl` command to generate a self-signed certificate.

---

## Debug vs production summary

| Behaviour | Debug (`true`) | Production (`false`) |
|-----------|---------------|---------------------|
| Error hints in JSON | ✅ Shown | ❌ Hidden |
| 5xx logged to stderr | ✅ Yes | ❌ No |
| Startup banner | ✅ Shows "development" | Shows "production" |
