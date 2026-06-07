# Database Integration

Xpress++ provides a built-in Express-like wrapper around Drogon's non-blocking, asynchronous database engine. This allows you to connect to SQL databases (PostgreSQL, MySQL, SQLite3) and perform performant coroutine queries with simple `async` and `await` syntax.

---

## Configuration (`xp::DbConfig`)

Configure your database connection using `xp::DbConfig` before calling `app.listen()`.

```cpp
xp::App app;

xp::DbConfig db_config;
db_config.driver = "postgresql";     // "sqlite3", "postgresql", or "mysql"
db_config.host = "127.0.0.1";        // Database host
db_config.port = 5450;               // Database port
db_config.database = "my_app";       // Database name
db_config.username = "postgres";     // Database user
db_config.password = "password";     // Database password
db_config.connection_number = 5;    // Connection pool size (default: 1)

app.database(db_config);
```

### Configuration Parameters

| Field | Type | Default | Description |
|-------|------|---------|-------------|
| `driver` | `string` | `"sqlite3"` | Connection driver (`postgresql`, `mysql`, `sqlite3`) |
| `host` | `string` | `"127.0.0.1"` | IP address or hostname of the database server |
| `port` | `int` | `0` (default) | Database port (`0` uses the default port for the selected driver) |
| `database` | `string` | `""` | Name of the database to connect to (or file path for sqlite3) |
| `username` | `string` | `""` | Database username |
| `password` | `string` | `""` | Database password |
| `connection_number` | `size_t` | `1` | Max number of concurrent connections in the connection pool |
| `name` | `string` | `"default"` | Logical name of this database client instance |

---

## Startup Initialization (`app.onStart`)

Often, you want to perform initialization tasks—such as verifying a connection, creating tables, or seeding mock data—when your application starts up. 

Use `app.onStart()` to run any asynchronous setup code before the HTTP server begins listening:

```cpp
app.onStart([]() async {
    try {
        // Run migrations/DDL asynchronously on startup
        await xp::query(
            "CREATE TABLE IF NOT EXISTS users ("
            "id SERIAL PRIMARY KEY, "
            "username VARCHAR(50) UNIQUE NOT NULL, "
            "password VARCHAR(100) NOT NULL"
            ");"
        );
        std::cout << "[DB Setup] 'users' table initialized successfully." << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "[DB Setup Failed] " << e.what() << std::endl;
    }
});
```

---

## Executing Asynchronous Queries (`await xp::query`)

Xpress++ routes declared as `async` can run non-blocking SQL queries using `await xp::query()`. This lets the worker threads continue processing other HTTP requests while waiting for the database server to reply.

### 1. Simple Select Queries

```cpp
app.get("/users", [](xp::Request& req, xp::Response& res) async {
    auto result = await xp::query("SELECT id, username FROM users;");
    res.json(xp::resultToJson(result));
});
```

### 2. Parameterized Queries (SQL Injection Prevention)

Never concatenate strings to build SQL queries. Instead, pass values as arguments. Xpress++ automatically escapes placeholders (`$1`, `$2`, etc.) securely:

```cpp
app.post("/users", [](xp::Request& req, xp::Response& res) async {
    auto body = req.json();
    auto username = body["username"].asString();
    auto password = body["password"].asString();

    await xp::query(
        "INSERT INTO users (username, password) VALUES ($1, $2);",
        username, password
    );

    res.json({{"success", true}});
});
```

---

## JSON Conversions

To make exposing database records over JSON APIs painless, Xpress++ provides helpers to convert database rows or full results to `Json::Value` output.

### `xp::resultToJson(result)`

Converts a full SQL result set (array of rows) into a JSON array:

```cpp
auto result = await xp::query("SELECT * FROM users;");
Json::Value json_array = xp::resultToJson(result);
res.json(json_array);
```

### `xp::rowToJson(result, row)`

Converts a single database row into a JSON object:

```cpp
auto result = await xp::query("SELECT * FROM users WHERE id = $1 LIMIT 1;", 42);
if (!result.empty()) {
    Json::Value json_user = xp::rowToJson(result, result[0]);
    res.json(json_user);
} else {
    res.status(404).json({{"error", "User not found"}});
}
```

---

## Multiple Database Connections

If your application needs to talk to more than one database, configure them with distinct names:

```cpp
// 1. Configure default postgres database
xp::DbConfig pg_config;
pg_config.driver = "postgresql";
pg_config.database = "prod_db";
app.database(pg_config);

// 2. Configure secondary read-replica database
xp::DbConfig replica_config;
replica_config.driver = "postgresql";
replica_config.database = "replica_db";
replica_config.name = "replica";
app.database(replica_config);
```

Then execute queries against specific database instances by specifying the database name:

```cpp
// Queries replica db
auto result = await xp::queryClient("replica", "SELECT * FROM products;");
```
