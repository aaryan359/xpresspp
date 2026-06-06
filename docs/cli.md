# xp CLI

The `xp` command-line tool manages your Xpress++ projects so you never have to write CMake commands by hand.

## Commands

### `xp create <app-name>`

Creates a new Xpress++ project in a new folder.

```bash
xp create my-api
```

This will:
- Copy the Xpress++ headers into `my-api/vendor/xpresspp/include/`
- Generate a ready-to-use `main.cpp` and `CMakeLists.txt`
- Print the next steps

**Rules for app names:** letters, numbers, hyphens (`-`), and underscores (`_`) only.

---

### `xp build`

Compiles the project in the current directory (Debug mode by default).

```bash
xp build
```

For a production-optimised build with `-O2`:

```bash
xp build --release
```

**What it does:**
1. Checks you're in an Xpress++ project directory
2. Detects and cleans stale CMake cache from other projects
3. Runs `cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug`
4. Runs `cmake --build build --parallel`
5. If compilation fails, analyses the output and prints targeted hints

---

### `xp run`

Builds the project and immediately runs the server.

```bash
xp run
```

```bash
xp run --release
```

---

### `xp watch`

Watches for source file changes and **automatically rebuilds** when you save:

```bash
xp watch
```

Monitors `.cpp`, `.h`, `.hpp`, and `CMakeLists.txt` files. If the build fails, it shows the error and waits for the next save.

::: tip Use watch during development
Run `xp watch` in one terminal and test your API in another. Every time you save, the rebuild happens automatically.
:::

---

### `xp clean`

Deletes the `./build` directory, forcing a clean rebuild on the next `xp build` or `xp run`.

```bash
xp clean
```

---

### `xp doctor`

Checks that all required system dependencies are installed and ready.

```bash
xp doctor
```

Example output:

```
  Xpress++ System Check
  ─────────────────────────────────────────
  ✓ cmake  cmake version 3.28.3
  ✓ C++ compiler  g++ (Ubuntu 13.3.0) 13.3.0
  ✓ Drogon library  (found)
  ✓ jsoncpp  (found)
  ✓ OpenSSL (TLS)  OpenSSL 3.0.13
  ✓ Xpress++ headers  /home/user/xpress++
  ─────────────────────────────────────────
  ✓ All checks passed — you are ready to build with Xpress++!
```

If something is missing, it shows exactly what to install:

```
  ⚠ Drogon — NOT FOUND
    Fix: https://github.com/drogonframework/drogon/wiki/ENG-02-Installation
    Ubuntu quick: sudo apt install libdrogon-dev
```

---

## Environment variables

The CLI uses these to locate the Xpress++ repo:

| Variable | Description |
|----------|-------------|
| `XPRESSPP_HOME` | Absolute path to the xpress++ repo root. **Always set this in production/CI.** |
| `XPRESSPP_CLI_PATH` | Path to the `xp` binary. Used as a fallback to auto-detect the repo root. |

```bash
export XPRESSPP_HOME=/opt/xpresspp
```

---

## Build output

When a build fails, the CLI analyses the compiler output and prints targeted hints:

| Symptom | Hint |
|---------|------|
| `undefined reference to ...` | A library isn't linked — check `target_link_libraries()` in CMakeLists.txt |
| `fatal error: someheader.h: No such file` | A header is missing — run `xp doctor` to verify your setup |
| `'xp::SomeClass' was not declared` | Likely a missing `#include <xpresspp/xpresspp.h>` |
| C++ standard errors | Ensure `set(CMAKE_CXX_STANDARD 20)` is in CMakeLists.txt |

---

## Quick reference

| Command | Description |
|---------|-------------|
| `xp create <name>` | Scaffold a new project |
| `xp build` | Debug build |
| `xp build --release` | Optimised production build |
| `xp run` | Build and run |
| `xp run --release` | Build (release) and run |
| `xp watch` | Auto-rebuild on source changes |
| `xp clean` | Delete build cache |
| `xp doctor` | Check dependencies |
