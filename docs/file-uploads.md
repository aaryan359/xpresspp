---
title: File Uploads
description: Read multipart form uploads through Drogon's multipart parser.
---

# File Uploads

Use `req.files()` or `req.file(name)` when a route expects multipart uploads.

```cpp
app.post("/upload", [](xp::Request& req, xp::Response& res) {
    auto avatar = req.file("avatar");
    if (!avatar) {
        res.badRequest("Missing avatar file");
        return;
    }

    avatar->save("./uploads");
    res.created({{"status", "uploaded"}});
});
```

Multipart parsing only happens when you call the upload helpers.
