# Xpress++ Public Sandbox API

This example is designed for a hosted "try Xpress++" endpoint. It gives users a
few safe API routes they can call from curl, Postman, browsers, or a benchmark
tool.

## Run Locally

```bash
cmake -S examples/sandbox_api -B examples/sandbox_api/build
cmake --build examples/sandbox_api/build --parallel
PORT=8080 examples/sandbox_api/build/xpresspp_sandbox_api
```

## Endpoints

```bash
curl http://localhost:8080/
curl http://localhost:8080/health
curl http://localhost:8080/api/stats
```

Echo JSON:

```bash
curl -X POST http://localhost:8080/api/echo \
  -H "Content-Type: application/json" \
  -d '{"hello":"xpress++"}'
```

Try the tiny AI demo endpoint:

```bash
curl -X POST http://localhost:8080/api/ai/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"What is Xpress++ good for?"}'
```

Check whether real local LLM mode is enabled:

```bash
curl http://localhost:8080/api/llm/status
```

Call the real local LLM proxy when configured:

```bash
curl -X POST http://localhost:8080/api/llm/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"Explain why someone would serve AI from C++."}'
```

## Add The Fixed Small LLM

The hosted sandbox is designed to expose one fixed small model:

```text
Qwen/Qwen2.5-0.5B-Instruct-GGUF:Q4_K_M
```

This keeps the machine requirement low and avoids letting public users choose
arbitrary models. Users only send prompts to your Xpress++ endpoint.

The recommended setup is:

1. Run Qwen2.5 0.5B Q4 with `llama.cpp`.
2. Keep `llama.cpp` bound to localhost.
3. Expose only the Xpress++ sandbox endpoint publicly.

The first start downloads/caches the model under `examples/sandbox_api/models/`.
The model file is intentionally ignored by git.

Terminal 1:

```bash
examples/sandbox_api/scripts/run_qwen_llm.sh
```

Terminal 2:

```bash
examples/sandbox_api/scripts/run_sandbox_with_qwen.sh
```

Now users can call:

```bash
curl -X POST https://your-domain.example/api/llm/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"What is Xpress++?"}'
```

## Hosting Notes

Set `PORT` for your server or platform:

```bash
PORT=9000 ./xpresspp_sandbox_api
```

Adjust the public sandbox rate limit:

```bash
SANDBOX_RATE_LIMIT=300 ./xpresspp_sandbox_api
```

The included `/api/ai/chat` endpoint uses a tiny deterministic demo responder so
the sandbox works without model files. The `/api/llm/chat` endpoint proxies to a
local OpenAI-compatible llama.cpp server running the fixed Qwen2.5 0.5B model
when `LLM_BASE_URL` is configured.

## Suggested Public Pitch

Expose this backend at a URL such as:

```text
https://sandbox.xpresspp.dev
```

Then share:

```text
Try the Xpress++ public sandbox:

curl https://sandbox.xpresspp.dev/health
curl https://sandbox.xpresspp.dev/api/stats
curl -X POST https://sandbox.xpresspp.dev/api/echo \
  -H "Content-Type: application/json" \
  -d '{"hello":"native C++ APIs"}'
curl -X POST https://sandbox.xpresspp.dev/api/ai/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"What can Xpress++ serve?"}'
curl -X POST https://sandbox.xpresspp.dev/api/llm/chat \
  -H "Content-Type: application/json" \
  -d '{"message":"Give me a tiny C++ API idea."}'
```
