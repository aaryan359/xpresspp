# Dependencies

Xpress++ depends on two libraries: **Drogon** (the HTTP engine) and **jsoncpp** (JSON parsing).

## Drogon

Drogon is the fastest C++ web framework, powering Xpress++ under the hood. You interact with it only through the Xpress++ API — you never need to write Drogon code directly.

### Install on Ubuntu / Debian

```bash
# Install Drogon's dependencies:
sudo apt install -y libssl-dev libjsoncpp-dev zlib1g-dev

# Build Drogon from source (recommended — apt package is often outdated):
git clone https://github.com/drogonframework/drogon
cd drogon
git submodule update --init
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_EXAMPLES=OFF
cmake --build build --parallel
sudo cmake --install build
```

### Install on macOS

```bash
brew install drogon
```

### Verify installation

```bash
xp doctor  # Shows ✓ Drogon library  (found)
```

---

## jsoncpp

Used for all JSON parsing and serialisation. Usually installed with Drogon, but if needed:

```bash
# Ubuntu / Debian
sudo apt install libjsoncpp-dev

# macOS
brew install jsoncpp

# Arch Linux
sudo pacman -S jsoncpp
```

---

## CMake versions

Xpress++ requires CMake 3.16 or newer.

```bash
cmake --version  # Must be >= 3.16

# Install on Ubuntu if too old:
sudo snap install cmake --classic
```

---

## C++ compiler

A C++20-capable compiler is required:

| Compiler | Minimum version |
|----------|----------------|
| GCC | 11+ |
| Clang | 13+ |
| MSVC | VS 2022 17.4+ |

```bash
# Install on Ubuntu:
sudo apt install g++

# Check version:
g++ --version
```

---

## Troubleshooting

Run `xp doctor` for a full automated check:

```bash
xp doctor
```

### Drogon not found by CMake

If CMake can't find Drogon even after installing it, the `DrogonConfig.cmake` file may be in a non-standard location:

```bash
find /usr /usr/local /opt -name 'DrogonConfig.cmake' 2>/dev/null
```

Then set `CMAKE_PREFIX_PATH`:

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/drogon/install
```

### jsoncpp not found

```bash
# Ubuntu
sudo apt install libjsoncpp-dev

# If still not found, check:
find /usr -name 'json.h' -path '*/jsoncpp/*'
```
