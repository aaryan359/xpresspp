# Static Files

Serve HTML, CSS, JavaScript, images, and other static assets from a directory.

## Basic usage

```cpp
// Serve everything in the ./public directory at the root URL
app.staticFiles("./public");
```

Now files in `./public` are available:
- `./public/index.html` → `http://localhost:8080/`
- `./public/style.css`  → `http://localhost:8080/style.css`
- `./public/app.js`     → `http://localhost:8080/app.js`

---

## Custom URL prefix

```cpp
// Serve from /assets/* instead of /*
app.staticFiles("./public", "/assets");
```

Now:
- `./public/logo.png` → `http://localhost:8080/assets/logo.png`

---

## Single Page Application (SPA) mode

For React, Vue, or Angular apps — serve `index.html` for any unknown path so client-side routing works:

```cpp
app.staticFiles("./dist", "/", true);  // true = SPA fallback
```

With SPA mode:
- `/` → `dist/index.html`
- `/about` → `dist/index.html` (not a 404 — let the JS router handle it)
- `/assets/app.js` → `dist/assets/app.js` (real file — served directly)

---

## Multiple directories

```cpp
app.staticFiles("./public",   "/");
app.staticFiles("./uploads",  "/files");
app.staticFiles("./docs",     "/documentation");
```

---

## MIME types

MIME types are detected automatically from file extensions:

| Extension | MIME Type |
|-----------|-----------|
| `.html`, `.htm` | `text/html; charset=utf-8` |
| `.css` | `text/css; charset=utf-8` |
| `.js` | `application/javascript; charset=utf-8` |
| `.json` | `application/json; charset=utf-8` |
| `.png` | `image/png` |
| `.jpg`, `.jpeg` | `image/jpeg` |
| `.gif` | `image/gif` |
| `.svg` | `image/svg+xml` |
| `.txt` | `text/plain; charset=utf-8` |
| `.pdf` | `application/pdf` |
| other | `application/octet-stream` |

::: tip Directory fallback
If a URL points to a directory, Xpress++ automatically looks for `index.html` inside it.
:::

::: warning Static files directory missing
If the directory you pass to `staticFiles()` doesn't exist, Xpress++ will print a warning in the terminal. It won't crash — requests to that prefix will just fall through to your route handlers.
:::
