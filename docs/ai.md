# AI Interface

Xpress++ can expose native C++ AI workloads as clean HTTP APIs. The framework
does not ship a model runtime. Bring your own llama.cpp, ONNX Runtime,
Whisper.cpp, OpenCV, TensorRT, or custom inference code, then let Xpress++
handle the request and response layer.

The first helper is `xp::ai::chat`, a small route adapter for JSON chat-style
endpoints.

## Chat Helper

```cpp
#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    app.post("/chat", xp::ai::chat([](const xp::ai::ChatRequest& input) {
        return "Native model reply for: " + input.message;
    }));

    app.listen(8080);
}
```

Send a request:

```bash
curl -X POST http://localhost:8080/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"Explain C++ coroutines simply","model":"local-demo"}'
```

Response:

```json
{
  "reply": "Native model reply for: Explain C++ coroutines simply",
  "model": "local-demo",
  "durationMs": 0
}
```

## Return Shapes

The callback can return a `std::string`, `Json::Value`, or
`xp::ai::ChatResponse`.

```cpp
app.post("/chat", xp::ai::chat([&](const xp::ai::ChatRequest& input) {
    xp::ai::ChatResponse output;
    output.reply = model.generate(input.message);
    output.model = "llama.cpp";
    output.metadata["tokens"] = 128;
    return output;
}));
```

## Request Shape

`xp::ai::chat` accepts either `message` or `prompt`:

```json
{
  "message": "Write a short summary",
  "model": "my-local-model"
}
```

If the body is not a JSON object or the message is empty, Xpress++ returns a
`400 Bad Request` response.

## Options

```cpp
xp::ai::ChatOptions options;
options.messageField = "input";
options.responseField = "answer";
options.defaultModel = "local";
options.includeDuration = true;

app.post("/ask", xp::ai::chat(modelHandler, options));
```

## Completion Helper

For completion-style APIs, `xp::ai::completion` uses the same request parsing
but writes the generated text to `completion` by default.

```cpp
app.post("/complete", xp::ai::completion([](std::string prompt) {
    return completeText(prompt);
}));
```

## What This Is For

Use this layer when your actual model or processing code already lives in C++:

- local LLM wrappers
- image classification
- audio transcription
- embedding generation
- robotics or edge inference
- custom OpenCV/CUDA pipelines

The goal is simple: keep model ownership in your C++ code and avoid writing
repeated HTTP glue for every inference endpoint.
