# Xpress++ Future Roadmap & Plans

This document outlines the planned roadmap and future capabilities for the Xpress++ framework, focusing on developer experience (DX), ecosystem growth, and performance.

---

## 📦 1. Third-Party Package Ecosystem (`xp install`)

To match the modular design of Express.js, Xpress++ will support a decentralized package registry where third-party developers can publish middleware, database adapters, and utilities (e.g., `xp-helmet`, `xp-cors`, `xp-session`).

### Developer Workflow
Users will be able to install packages directly via the CLI:
```bash
xp install xp-helmet
```

### Under the Hood
1. **Dependency Registration**: The CLI resolves the package name, fetches its remote repository metadata, and automatically appends it to the project's `vcpkg.json` or modifies the `CMakeLists.txt` to include it via `FetchContent`.
2. **Auto-Compiling**: The next time `xp watch` runs or the project builds, CMake automatically downloads, caches, compiles, and links the new package.
3. **Usage**:
   ```cpp
   #include <xp_helmet.h>
   app.use(helmet());
   ```

---

## 🔌 2. Native WebSocket Support

Providing a clean, Express-like WebSocket handler layer sitting on top of Drogon's underlying WebSocket framework.

### Proposed Syntax
```cpp
app.ws("/chat", [](xp::WebSocketConn& ws) {
    ws.onMessage([](std::string message) {
        ws.send("Received: " + message);
    });
    ws.onClose([]() {
        std::cout << "Connection closed\n";
    });
});
```

---

## 🔒 3. ORM Security & Relations

### Parameterized Queries
- Shift the ORM internals (`Model::find`, `Model::create`, etc.) from raw string concatenation (`escapeValue`) to parameterized SQL statement execution (`execSqlCoro`) to eliminate SQL injection risks.

### Schema Relations
- Enhance `schema.xp` to support FK relationships (`@relation`), compound indices, and auto-generated join tables.

---

## ⚡ 4. Async Middleware

Currently, the middleware pipeline runs synchronously. Supporting asynchronous middleware execution will allow developers to run database queries or perform network requests (e.g. session checks, API authentication) inside middleware using coroutines:

```cpp
using AsyncMiddleware = std::function<Task<void>(Request&, Response&, Next)>;
```

---

## 🛠️ 5. Richer CLI Scaffolding (`xp generate`)

Generate components, routes, and controllers automatically, speeding up new feature development:

```bash
xp generate controller Users
xp generate route /api/users
xp generate model Post
```
