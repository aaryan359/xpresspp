# Installation

Get your first Xpress++ server running in under five minutes.

## Prerequisites

You need the following tools installed before you begin:

| Tool | Version | Purpose |
|------|---------|---------|
| **C++ compiler** | GCC 11+ or Clang 14+ | Compiles your code |
| **CMake** | 3.16+ | Build system |
| **Drogon** | Latest | The HTTP engine under the hood |
| **jsoncpp** | Any | JSON parsing |

::: tip Check everything at once
Run `xp doctor` after installing the CLI and it will check all dependencies for you.
:::

## Step 1 — Install system dependencies

### Ubuntu / Debian

```bash
# Build tools
sudo apt update
sudo apt install -y build-essential cmake git

# Drogon dependencies
sudo apt install -y libjsoncpp-dev libssl-dev zlib1g-dev

# Install Drogon from source (the apt package is often outdated)
git clone https://github.com/drogonframework/drogon
cd drogon && git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
sudo cmake --install build
```

### macOS (Homebrew)

```bash
brew install cmake jsoncpp openssl
brew install drogon
```

### Arch Linux

```bash
sudo pacman -S base-devel cmake jsoncpp openssl
yay -S drogon   # or build from source
```

---

## Step 2 — Install the xp CLI

The `xp` CLI is the easiest way to create and manage Xpress++ projects.

```bash
# Clone the Xpress++ repo
git clone https://github.com/aaryan359/xpresspp
cd xpresspp

# Build the CLI
cmake -S cli -B cli/build -DCMAKE_BUILD_TYPE=Release
cmake --build cli/build

# Install it globally
sudo cp cli/build/xp /usr/local/bin/xp
```

Verify it works:

```bash
xp
```

You should see:

```
  Xpress++  —  Dragon-speed, Express-simplicity

  Usage:
    xp create  <app-name>   Create a new Xpress++ project
    xp build   [--release]  Compile the current project
    xp run     [--release]  Build and run the server
    xp watch                Auto-rebuild on source changes
    xp clean                Delete the build directory
    xp doctor               Check system dependencies
```

---

## Step 3 — Create your first app

```bash
xp create my-app
cd my-app
xp run
```

That's it. Your server is now running at **http://localhost:8080**.

Test it:

```bash
curl http://localhost:8080/api/ping
# {"framework":"Xpress++","status":"healthy"}
```

---

## What just happened?

`xp create` scaffolded a project with this structure:

```
my-app/
├── main.cpp              ← Your application code
├── CMakeLists.txt        ← Build configuration (managed by xp)
├── vendor/
│   └── xpresspp/
│       └── include/      ← Xpress++ headers (copied from the repo)
└── uploads/              ← Default upload directory
```

`xp run` then:
1. Configured CMake in `./build`
2. Compiled `main.cpp`
3. Launched the binary

When you change `main.cpp` and save, run `xp watch` to rebuild automatically.

---

## Your first route

Open `main.cpp` and add a route:

```cpp
#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    app.get("/", [](xp::Request& req, xp::Response& res) {
        res.json({
            {"message", "Hello from Xpress++!"}
        });
    });

    app.get("/users/:id", [](xp::Request& req, xp::Response& res) {
        const auto id = req.param("id");
        res.json({
            {"userId", id},
            {"name",   "Alice"}
        });
    });

    app.listen(8080);
    return 0;
}
```

Run it with `xp run`, then test:

```bash
curl http://localhost:8080/users/42
# {"name":"Alice","userId":"42"}
```

---

## Next steps

- [Routing guide](/routing) — Learn all the ways to define routes
- [Request](/request) — Read headers, body, params, cookies
- [Response](/response) — Send JSON, HTML, files, redirects
- [Error handling](/error-handling) — Throw typed errors and handle them globally
- [Middleware](/middleware) — Add CORS, auth, logging and more
