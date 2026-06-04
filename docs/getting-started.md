---
title: Getting Started
description: Create and run your first Xpress++ application.
---

# Getting Started

Xpress++ is a C++ web framework with an Express-style API and a Drogon-powered runtime.

## Create an app

```bash
xp create my-api
cd my-api
xp run
```

## Minimal server

```cpp
#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    app.get("/api/ping", [](xp::Request& req, xp::Response& res) {
        res.json({
            {"status", "healthy"},
            {"framework", "Xpress++"}
        });
    });

    app.listen(8080);
}
```

## Generated project

```txt
my-api/
  CMakeLists.txt
  main.cpp
  vcpkg.json
  vendor/
    xpresspp/
      include/
```

The generated app is intentionally small. The framework headers are vendored locally so the app can compile without relying on a global Xpress++ install.
