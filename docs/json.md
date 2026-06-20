# JSON & Dynamic Types (`xp::var`)

Xpress++ features a custom JSON wrapper type called **`xp::var`**. This type inherits from [jsoncpp](https://github.com/open-source-parsers/jsoncpp)'s `Json::Value` but adds implicit conversion operators, allowing you to write clean C++ code that reads and acts exactly like JavaScript/Node.js.

---

## 1. Dynamic Variables with `auto`

Normally in C++, extracting values from a JSON object requires calling verbose methods like `.asString()` or `.asInt()`. With Xpress++, `req.json()` returns an `xp::var` instance. You can assign fields directly to core C++ types or use `auto` and rely on implicit conversions.

### Traditional Verbose C++:
```cpp
app.post("/users", [](xp::Request& req, xp::Response& res) {
    const auto body = req.json();

    const string name = body["name"].asString();
    const int age     = body["age"].asInt();
    const bool admin  = body["admin"].asBool();
    const double score = body["score"].asDouble();

    res.json({{"received", name}});
});
```

### Clean Xpress++ Way (Implicit Conversions):
```cpp
app.post("/users", [](xp::Request& req, xp::Response& res) {
    // 1. Deduces to xp::var
    auto body = req.json();

    // 2. Implicit conversion to core types! No .asString() / .asInt() needed.
    string name  = body["name"];
    int age      = body["age"];
    bool admin   = body["admin"];
    double score = body["score"];

    res.json({{"received", name}});
});
```

### Using `auto` vs. Explicit Types

You can use either explicit types (`string`, `int`, etc.) or `auto`. Both are valid, but behave slightly differently:

#### 1. Using `auto` (Deferred Conversion)
If you declare variables using `auto`, the type is deduced as `xp::var`. The actual conversion will happen automatically later when you pass the variable to a function expecting a specific type:

```cpp
auto username = body["username"]; // Deduces to xp::var
auto age      = body["age"];      // Deduces to xp::var

// Pass directly to functions; implicit casting happens at the call site:
someFunctionExpectingString(username); 
someFunctionExpectingInt(age);
```

#### 2. Using Explicit Types (Immediate Conversion)
If you declare variables with explicit C++ types, the conversion happens immediately:

```cpp
string username = body["username"]; // Converts to string immediately
int age          = body["age"];      // Converts to int immediately
```

> [!NOTE]
> The only time you must use explicit types (or casts) is when performing operations where the compiler cannot determine the target type automatically, such as concatenating strings directly:
> ```cpp
> // ❌ Error: Compiler doesn't know if you want string or numeric addition
> auto val = body["name"] + " suffix"; 
> 
> //  Correct:
> string val = body["name"];
> val += " suffix";
> ```

---

## 2. Reading Nested Objects and Arrays

Because `xp::var` overrides `operator[]` to return another `xp::var` instance, chained lookups preserve dynamic typing and implicit conversion:

```cpp
auto body = req.json();

// Nested lookup converts implicitly
string city = body["address"]["city"];

// Iterate over arrays
for (const auto& tag : body["tags"]) {
    // tag is deduced as xp::var, converts implicitly to string
    string tagName = tag;
    std::cout << tagName << "\n";
}

// Array size
size_t count = body["items"].size();
```

---

## 3. Implicit Casting Rules

When converting `xp::var` to C++ primitive types, conversions are resilient to prevent server crashes:

*   **To `std::string`**:
    *   Null values convert to `""`.
    *   JSON strings, numbers, and booleans convert to their string representations.
*   **To `int` / `double`**:
    *   Null values convert to `0` / `0.0`.
    *   Numeric values convert directly.
    *   JSON string values like `"42"` or `"12.34"` are automatically parsed using `std::stoi` / `std::stod` under the hood. If parsing fails, they fall back to `0` / `0.0` safely.
*   **To `bool`**:
    *   Null values convert to `false`.
    *   Booleans and non-zero numbers convert directly.
    *   Strings like `"true"`, `"1"`, or `"yes"` convert to `true`. All other strings convert to `false`.

---

## 4. Sending JSON Responses

### Inline initializer list using `xp::obj` / `xp::arr` (simplest)
```cpp
// res.json takes an xp::obj/xp::var initializer list
res.json({
    {"id",     42},
    {"name",   "Alice"},
    {"active", true}
});
```

### Building a `Json::Value` / `xp::var` manually:
```cpp
xp::var user;
user["id"]    = 42;
user["name"]  = "Alice";
user["email"] = "alice@example.com";

res.json(user);
```

### Nesting with `xp::obj` and `xp::arr`:
```cpp
res.json({
    {"status", "healthy"},
    {"payload", xp::obj({
        {"items", xp::arr({1, 2, 3})},
        {"metadata", xp::obj({{"count", 3}})}
    })}
});
```

---

## 5. jsoncpp Compatibility Reference

Because `xp::var` inherits from `Json::Value`, all standard jsoncpp verification methods remain fully supported:

| Method | Return Type | Description |
|--------|-------------|-------------|
| `.isNull()` | `bool` | Returns true if the field is null |
| `.isMember(key)` | `bool` | Returns true if object has this key |
| `.size()` | `size_t` | Length of array or number of object keys |
| `.isArray()` | `bool` | True if the node is a JSON array |
| `.isObject()` | `bool` | True if the node is a JSON object |
| `.isString()` | `bool` | True if the node is a string |
| `.isInt()` | `bool` | True if the node is a 32-bit signed integer |
| `.isNumeric()` | `bool` | True if the node is a number |
| `.isBool()` | `bool` | True if the node is a boolean |
