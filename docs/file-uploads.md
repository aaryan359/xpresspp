# File Uploads

Accept files from multipart form submissions.

## Basic example

```cpp
app.post("/upload", [](xp::Request& req, xp::Response& res) {
    // Get a specific file by form field name
    const auto file = req.file("avatar");

    if (!file) {
        throw xp::BadRequestError("No file was uploaded in the 'avatar' field.");
    }

    // Save the file
    const std::string savePath = "uploads/" + file->getFileName();
    file->saveAs(savePath);

    res.json({
        {"message",  "File uploaded"},
        {"filename", file->getFileName()},
        {"size",     std::to_string(file->fileLength()) + " bytes"}
    });
});
```

---

## Multiple files

```cpp
app.post("/upload-many", [](xp::Request& req, xp::Response& res) {
    const auto files = req.files();  // vector<drogon::HttpFile>

    if (files.empty()) {
        throw xp::BadRequestError("No files uploaded.");
    }

    Json::Value uploaded(Json::arrayValue);
    for (const auto& file : files) {
        file.saveAs("uploads/" + file.getFileName());
        uploaded.append(file.getFileName());
    }

    res.json({{"uploaded", uploaded}});
});
```

---

## Validate file type and size

```cpp
app.post("/upload-image",
    {xp::bodyLimit(5 * 1024 * 1024)},  // Max 5 MB total body
    [](xp::Request& req, xp::Response& res) {
        const auto file = req.file("image");
        if (!file) {
            throw xp::BadRequestError("No image provided.");
        }

        // Check extension
        const auto name = file->getFileName();
        const auto ext  = name.substr(name.rfind('.'));
        if (ext != ".jpg" && ext != ".jpeg" && ext != ".png" && ext != ".gif") {
            throw xp::BadRequestError(
                "Invalid file type. Only JPG, PNG, and GIF are allowed."
            );
        }

        file->saveAs("uploads/" + name);
        res.json({{"message", "Image uploaded"}, {"filename", name}});
    }
);
```

---

## Frontend HTML form

```html
<form action="/upload" method="POST" enctype="multipart/form-data">
    <input type="file" name="avatar" />
    <button type="submit">Upload</button>
</form>
```

Or with JavaScript:

```javascript
const formData = new FormData();
formData.append('avatar', fileInput.files[0]);

fetch('/upload', {
    method: 'POST',
    body: formData
});
```

---

## Useful file methods

| Method | Returns | Description |
|--------|---------|-------------|
| `file->getFileName()` | `string` | Original filename |
| `file->fileLength()` | `size_t` | File size in bytes |
| `file->saveAs(path)` | `int` | Save to disk (returns 0 on success) |
| `file->getFileExtension()` | `string` | File extension (e.g. `"jpg"`) |
| `file->getMd5()` | `string` | MD5 hash of file contents |

---

## Creating the uploads directory

Make sure the uploads directory exists before running your server:

```cpp
#include <filesystem>

int main() {
    std::filesystem::create_directories("uploads");

    xp::App app;
    // ...
}
```
