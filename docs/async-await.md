# Async & Await (Coroutines)

Xpress++ leverages native C++20 coroutines to deliver maximum performance while keeping code clean and developer-friendly. By abstracting away complex compiler keywords, Xpress++ lets you write asynchronous, non-blocking code using familiar JavaScript/Node.js-style `async` and `await` syntax.

---

## Why Use Async/Await?

In traditional C++, network and database queries block the executing thread. To scale to thousands of concurrent requests, developers had to write verbose nested callbacks.

With **Xpress++ Asynchronous Coroutines**:
1. **Non-blocking execution**: When you `await` a database query or external API call, the worker thread is released to handle other incoming requests.
2. **Synchronous readability**: Your code reads sequentially from top to bottom, avoiding callback hell and complex promise chains.
3. **No runtime overhead**: Native compiler-level coroutines compile down to state machines with zero call-stack overhead, running at near-bare-metal speed.

---

## Syntax Comparison

### Traditional C++20 Coroutines
```cpp
// Verbose, boilerplate-heavy syntax
app.get("/users", [](xp::Request& req, xp::Response& res) -> xp::Task<void> {
    try {
        auto result = co_await xp::query("SELECT name FROM users;");
        res.json(xp::resultToJson(result));
    } catch (const std::exception& e) {
        res.status(500).json({{"error", e.what()}});
    }
    co_return;
});
```

### Xpress++ Simplified Syntax
```cpp
// Clean, Express-like async/await syntax
app.get("/users", [](xp::Request& req, xp::Response& res) async {
    try {
        auto result = await xp::query("SELECT name FROM users;");
        res.json(xp::resultToJson(result));
    } catch (const std::exception& e) {
        res.status(500).json({{"error", e.what()}});
    }
});
```

---

## Three Golden Rules of Async/Await

To use coroutines in Xpress++ correctly, always follow these three rules:

### Rule 1: Declare the function or lambda as `async`
Any function or lambda containing `await` must be marked with the `async` keyword:
```cpp
// Correct: Lambda marked with async
app.get("/data", [](xp::Request& req, xp::Response& res) async {
    // ...
});
```

### Rule 2: Return type must be a Task
If you are writing a custom helper function that uses `async`, its return type must be wrapped in `xp::Task<T>` (or simply `xp::Task` if it doesn't return a value):
```cpp
// A custom async helper function
xp::Task<std::string> fetchUserEmail(int userId) async {
    auto result = await xp::query("SELECT email FROM users WHERE id = $1 LIMIT 1;", userId);
    if (result.empty()) {
        co_return "";
    }
    co_return result[0]["email"].as<std::string>();
}
```

### Rule 3: Use `co_return` instead of `return`
Inside any code block marked as `async`, standard C++ `return` is invalid. You must use **`co_return`** to return values or exit early:
```cpp
app.get("/check-user", [](xp::Request& req, xp::Response& res) async {
    auto username = req.query("username");
    if (username.empty()) {
        res.status(400).json({{"error", "Missing username"}});
        co_return; // Exits the async coroutine safely
    }

    auto result = await xp::query("SELECT id FROM users WHERE username = $1;", username);
    res.json({{"exists", !result.empty()}});
});
```

---

## Practical Examples

### 1. Sequential Async Calls
You can chain multiple `await` calls sequentially inside your handler:

```cpp
app.get("/api/v1/profile", [](xp::Request& req, xp::Response& res) async {
    auto userId = req.query("id");

    // Fetch user details first
    auto userResult = await xp::query("SELECT * FROM users WHERE id = $1;", userId);
    if (userResult.empty()) {
        res.status(404).json({{"error", "User not found"}});
        co_return;
    }

    // Fetch logs using info from the user result
    auto logsResult = await xp::query("SELECT * FROM audit_logs WHERE user_id = $1;", userId);

    res.json({
        {"user", xp::rowToJson(userResult, userResult[0])},
        {"logs", xp::resultToJson(logsResult)}
    });
});
```

### 2. Startup Tasks (`app.onStart`)
You can register async callbacks during application startup. This is perfect for setting up database schemas, seeds, or warming up caches:

```cpp
app.onStart([]() async {
    std::cout << "Starting database initialization..." << std::endl;
    
    await xp::query(
        "CREATE TABLE IF NOT EXISTS system_config ("
        "key VARCHAR(50) PRIMARY KEY, "
        "value VARCHAR(255) NOT NULL"
        ");"
    );
    
    std::cout << "System ready!" << std::endl;
});
```
