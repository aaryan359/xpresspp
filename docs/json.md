# JSON

Xpress++ uses [jsoncpp](https://github.com/open-source-parsers/jsoncpp) for JSON parsing and serialisation. Everything is built in — no extra setup needed.

## Reading JSON from a request

```cpp
app.post("/users", [](xp::Request& req, xp::Response& res) {
    const auto body = req.json();  // returns Json::Value

    // Read fields
    const auto name  = body["name"].asString();
    const auto age   = body["age"].asInt();
    const auto admin = body["admin"].asBool();
    const auto score = body["score"].asDouble();

    res.json({{"received", name}});
});
```

::: tip Check field existence first
Use `body.isMember("fieldName")` before reading to avoid exceptions on missing fields.
:::

---

## Reading nested objects and arrays

```cpp
const auto body = req.json();

// Nested object
const auto city = body["address"]["city"].asString();

// Array
for (const auto& tag : body["tags"]) {
    std::cout << tag.asString() << "\n";
}

// Array size
const auto count = body["items"].size();
```

---

## Sending JSON responses

### Inline initializer list (simplest)

```cpp
res.json({
    {"id",     42},
    {"name",   "Alice"},
    {"active", true}
});
```

### Building a `Json::Value` manually

```cpp
Json::Value user;
user["id"]    = 42;
user["name"]  = "Alice";
user["email"] = "alice@example.com";

res.json(user);
```

### Arrays

```cpp
Json::Value users(Json::arrayValue);

Json::Value user1;
user1["id"]   = 1;
user1["name"] = "Alice";
users.append(user1);

Json::Value user2;
user2["id"]   = 2;
user2["name"] = "Bob";
users.append(user2);

res.json(users);
```

Response:

```json
[
  {"id": 1, "name": "Alice"},
  {"id": 2, "name": "Bob"}
]
```

---

## Checking types safely

```cpp
const auto body = req.json();

if (!body.isMember("name") || !body["name"].isString()) {
    throw xp::BadRequestError("'name' must be a string.");
}
if (!body.isMember("age") || !body["age"].isInt()) {
    throw xp::BadRequestError("'age' must be an integer.");
}
```

---

## jsoncpp type conversion reference

| Method | C++ type | JSON type |
|--------|----------|-----------|
| `.asString()` | `std::string` | string |
| `.asInt()` | `int` | number |
| `.asInt64()` | `int64_t` | number |
| `.asUInt()` | `unsigned int` | number |
| `.asDouble()` | `double` | number |
| `.asBool()` | `bool` | boolean |
| `.isNull()` | `bool` | `null` check |
| `.isMember(key)` | `bool` | Object key check |
| `.size()` | `size_t` | Array/object length |
| `.isArray()` | `bool` | Array type check |
| `.isObject()` | `bool` | Object type check |
| `.isString()` | `bool` | String type check |
| `.isInt()` | `bool` | Int type check |
| `.isBool()` | `bool` | Bool type check |
