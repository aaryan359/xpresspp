# Database Integration (Prisma-like ORM)

Xpress++ features a native database-agnostic ORM that supports **PostgreSQL** and **MongoDB** out of the box. Database clients are compiled dynamically from your schema definition, completely hidden from your source tree, and fully integrated with C++ coroutine syntax sugar.

---

## 1. Schema Definition (`schema.xp`)

Create a `schema.xp` file at the root of your project to define your datasource provider and data models.

### PostgreSQL Configuration Example:
```prisma
datasource db {
  provider = "postgresql"
}

model User {
  id        Int      @id @default(autoincrement())
  username  String   @unique
  password  String
  createdAt DateTime @default(now())
}
```

### MongoDB Configuration Example:
```prisma
datasource db {
  provider = "mongodb"
}

model Product {
  id        String   @id @default(uuid())
  name      String   @unique
  price     Double
  active    Boolean  @default(true)
}
```

### Supported Data Types & Attributes:

| Prisma Type | C++ Map | DB Type (Postgres) | DB Type (MongoDB) |
|-------------|---------|--------------------|-------------------|
| `Int`       | `int`   | `INTEGER`          | `Int32`           |
| `String`    | `string`| `VARCHAR(255)`     | `String`          |
| `Boolean`   | `bool`  | `BOOLEAN`          | `Boolean`         |
| `Double`    | `double`| `DOUBLE PRECISION` | `Double`          |
| `DateTime`  | `string`| `TIMESTAMP`        | `Date`            |

*   `@id`: Marks the primary key/unique identifier of the model.
*   `@default(autoincrement())`: PostgreSQL serial column (only for `Int` primary keys).
*   `@default(uuid())`: Generates a dynamic UUID for the primary key (ideal for MongoDB).
*   `@default(now())`: Automatically populates the field with the current system timestamp.
*   `@unique`: Enforces unique constraints at the database level.

---

## 2. Generate and Migrate the DB Client

To compile your schema definition and synchronize it with the database, run:

```bash
xp migrate
```

### What this does:
1. Parses `schema.xp` and validates syntax, field types, and attributes.
2. Synchronizes database tables or collections matching the defined models.
3. Generates the unified, type-safe C++ Prisma-like client dynamically under the framework's vendor directory:
   ```
   vendor/xpresspp/include/xpresspp/db.h
   ```
4. Exposes the global **`prisma`** client instance instantly in your application, requiring **zero import boilerplate**.

---

## 3. Querying the Database

Xpress++ routes declared as `async` can run non-blocking database queries using the global `prisma` client.

> [!IMPORTANT]
> To avoid GCC 13 coroutine code generator compiler bugs, always declare the query as a local `xp::obj` variable before passing it to database methods.

### 1. Creating a Record (`create`)
```cpp
app.post("/users", [](xp::Request& req, xp::Response& res) async {
    try {
        // Read JSON body directly
        auto body = req.json();
        
        await prisma.user.create(body);
        res.created({{"success", true}});
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```

### 2. Finding a Record by Unique Identifier (`findUnique`)
```cpp
app.get("/users/:id", [](xp::Request& req, xp::Response& res) async {
    try {
        const auto id = std::stoi(req.param("id"));
        
        xp::obj query = {
            {"where", xp::obj{
                {"id", id}
            }}
        };
        
        auto user = await prisma.user.findUnique(query);
        if (!user.isNull()) {
            res.ok(user);
        } else {
            res.notFound("User not found");
        }
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```

### 3. Finding Many Records with Complex Filters (`findMany`)
Xpress++ supports MongoDB and SQL parameterized query operators securely:
*   `equals`, `not`
*   `gt`, `gte`, `lt`, `lte`
*   `contains`, `startsWith`, `endsWith`
*   `in`, `notIn`

```cpp
app.get("/users", [](xp::Request& req, xp::Response& res) async {
    try {
        xp::obj query = {
            {"where", xp::obj{
                {"age", xp::obj{
                    {"gt", 18}
                }},
                {"status", "active"}
            }}
        };
        
        auto users = await prisma.user.findMany(query);
        res.ok(users);
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```

### 4. Updating Records (`update`)
```cpp
app.patch("/users/:id", [](xp::Request& req, xp::Response& res) async {
    try {
        const auto id = std::stoi(req.param("id"));
        
        xp::obj updateQuery = {
            {"where", xp::obj{{"id", id}}},
            {"data", xp::obj{{"username", req.json()["username"]}}}
        };
        
        await prisma.user.update(updateQuery);
        res.ok({{"success", true}});
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```

### 5. Deleting Records (`deleteMany`)
```cpp
app.del("/users", [](xp::Request& req, xp::Response& res) async {
    try {
        xp::obj deleteQuery = {
            {"where", xp::obj{
                {"status", "inactive"}
            }}
        };
        
        await prisma.user.deleteMany(deleteQuery);
        res.ok({{"success", true}});
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```

---

## 4. Startup Synchronization

To ensure that your database connections are checked and new tables/collections are created at server launch, include `SchemaSync::syncAll()` inside the application `onStart` block in `main.cpp`:

```cpp
int main() {
    xp::loadEnv();
    xp::App app;
    
    // Set database connection string (from environment variable or inline)
    app.database("postgresql://postgres:postgres@localhost:5432/testdb");
    
    // Auto-migrate on startup
    app.onStart([]() async {
        try {
            co_await SchemaSync::syncAll();
            std::cout << "[DB] Schema sync completed successfully." << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[DB Error] Sync failed: " << e.what() << std::endl;
        }
    });

    app.listen();
    return 0;
}
```
