---
title: Static Files
description: Serve public assets and single-page applications.
---

# Static Files

Serve a directory from your Xpress++ app:

```cpp
app.staticFiles("./public");
```

## SPA fallback

```cpp
app.staticFiles("./dist", "/", true);
```

The third argument enables fallback to `index.html`, which is useful for single-page frontend apps.
