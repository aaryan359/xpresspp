# Database Integration (Xpress++ Database Client)

Xpress++ features a native database-agnostic client engine that supports **PostgreSQL** and **MongoDB** out of the box. Database clients are compiled dynamically from your schema definition, completely hidden from your source tree, and fully integrated with C++ coroutine syntax sugar.

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
2. Generates database migrations (`up.sql` and `down.sql`) inside a timestamped directory under `migrations/`.
3. Generates the unified, type-safe C++ client dynamically in your build folder:
   ```
   build/generated/db.h
   ```
   *(This ensures that your `src/` directory remains 100% clean and free of generated boilerplate).*
4. Exposes the global **`xpd`** client instance instantly in your application, requiring **zero import boilerplate**.

---

## 3. Database Migrations and Rollbacks

Xpress++ automatically tracks and runs migrations to keep your database schema in sync.

### A. Automatic Migration Sync
During startup, when `SchemaSync::syncAll()` is executed:
- The framework creates a tracking table named `_xp_migrations` if it doesn't already exist.
- It scans the `migrations/` folder, sorting and running any outstanding migrations (by applying their `up.sql`).
- Once successful, the applied migration's identity is stored in the database.

### B. Rollback Command (`xp migrate rollback`)
If you need to roll back the most recent migration:

```bash
xp migrate rollback
```

**Under the Hood:**
1. The `xp` CLI builds your project and runs the compiled binary with the `--rollback` (or `-r`) flag.
2. The application intercepts execution before starting the HTTP server or displaying the console banner.
3. The engine connects to the database, reads the last migration from `_xp_migrations`, applies its `down.sql` rollback sequence, and removes the entry.
4. The process exits cleanly (`exit(0)`) without binding to any network ports.

---

## 4. Querying the Database

Xpress++ routes declared as `async` can run non-blocking database queries using the global `xpd` client.

> [!IMPORTANT]
> To avoid GCC 13 coroutine code generator compiler bugs, always declare the query as a local `xp::var` variable before passing it to database methods.

### 1. Creating a Record (`create`)
```cpp
app.post("/users", [](xp::Request& req, xp::Response& res) async {
    try {
        // Read JSON body directly
        auto body = req.json();
        
        co_await xpd.user.create(body);
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
        
        xp::var query = {
            {"where", {
                {"id", id}
            }}
        };
        
        auto user = co_await xpd.user.findUnique(query);
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
        xp::var query = {
            {"where", {
                {"age", {
                    {"gt", 18}
                }},
                {"status", "active"}
            }}
        };
        
        auto users = co_await xpd.user.findMany(query);
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
        
        xp::var updateQuery = {
            {"where", {{"id", id}}},
            {"data", {{"username", req.json()["username"]}}}
        };
        
        co_await xpd.user.update(updateQuery);
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
        xp::var deleteQuery = {
            {"where", {
                {"status", "inactive"}
            }}
        };
        
        co_await xpd.user.deleteMany(deleteQuery);
        res.ok({{"success", true}});
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```

---

## 5. ORM Relations & Associations

Xpress++ supports declaring associations (like **One-to-Many**, **Many-to-One**) inside your `schema.xp` file, which maps natively to foreign keys in relational databases. You can eager-load associated relations dynamically using query-time inclusions.

### 1. Declaring Relations in `schema.xp`

Use the standard Prisma `@relation` directive and fields array syntax to define associations:

```prisma
model User {
  id        Int      @id @default(autoincrement())
  username  String   @unique
  posts     Post[]   // One-to-Many association (virtual)
}

model Post {
  id        Int      @id @default(autoincrement())
  title     String
  authorId  Int      // Foreign key
  author    User     @relation(fields: [authorId], references: [id]) // Many-to-One association
}
```

### 2. Eager-Loading Nested Records

At query-time, you can pass an `include` object specifying which relations you want to eager-load. The generated ORM client will handle nested database queries automatically under the hood.

#### Eager Loading One-to-Many (`posts` on `User`)
```cpp
app.get("/users", [](xp::Request& req, xp::Response& res) async {
    try {
        xp::var query = {
            {"include", {
                {"posts", true} // Eager-load Alice's posts
            }}
        };
        
        auto users = co_await xpd.user.findMany(query);
        res.ok(users);
        /*
          Response JSON will be structured as:
          [
            {
              "id": 1,
              "username": "alice",
              "posts": [
                { "id": 101, "title": "First Post", "authorId": 1 },
                { "id": 102, "title": "Second Post", "authorId": 1 }
              ]
            }
          ]
        */
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```

#### Eager Loading Many-to-One (`author` on `Post`)
```cpp
app.get("/posts", [](xp::Request& req, xp::Response& res) async {
    try {
        xp::var query = {
            {"include", {
                {"author", true} // Eager-load the author user object
            }}
        };
        
        auto posts = co_await xpd.post.findMany(query);
        res.ok(posts);
        /*
          Response JSON will be structured as:
          [
            {
              "id": 101,
              "title": "First Post",
              "authorId": 1,
              "author": {
                "id": 1,
                "username": "alice"
              }
            }
          ]
        */
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```

### 3. Many-to-Many Relations (Explicit Junction Tables)

Xpress++ enforces **Explicit Junction Tables** (also known as join or pivot tables) for Many-to-Many relations. This design ensures optimal compiler-optimized SQL generation, eliminates hidden ORM performance traps, and allows you to attach custom attributes to the relationship itself.

#### Defining Many-to-Many in `schema.xp`
Instead of using implicit arrays on both sides, define a junction model (e.g., `UserPost`) that contains foreign keys referencing both targets:

```prisma
model User {
  id        Int        @id @default(autoincrement())
  username  String     @unique
  posts     UserPost[] // Virtual relationship referencing the junction table
}

model Post {
  id        Int        @id @default(autoincrement())
  title     String
  users     UserPost[] // Virtual relationship referencing the junction table
}

// Explicit Junction Table
model UserPost {
  id         Int      @id @default(autoincrement())
  userId     Int
  postId     Int
  user       User     @relation(fields: [userId], references: [id])
  post       Post     @relation(fields: [postId], references: [id])
  
  // Custom metadata (completely supported!)
  assignedAt DateTime @default(now())
}
```

#### Querying & Nested Eager-Loading
You can eagerly load the junction table and its parent objects recursively:

```cpp
app.get("/users", [](xp::Request& req, xp::Response& res) async {
    try {
        xp::var query = {
            {"include", {
                {"posts", true} // Eager-loads the junction entries
            }}
        };
        
        auto users = co_await xpd.user.findMany(query);
        res.ok(users);
        /*
          Response JSON will be structured as:
          [
            {
              "id": 1,
              "username": "alice",
              "posts": [
                {
                  "id": 501,
                  "userId": 1,
                  "postId": 101,
                  "assignedAt": "2026-06-27T12:00:00Z"
                }
              ]
            }
          ]
        */
    } catch (const std::exception& e) {
        res.serverError(e.what());
    }
});
```
