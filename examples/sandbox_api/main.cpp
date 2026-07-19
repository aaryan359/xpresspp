#include <xpresspp/xpresspp.h>

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <stdexcept>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string env(const char* key, const std::string& fallback) {
    const char* value = std::getenv(key);
    return value == nullptr || std::string(value).empty() ? fallback : std::string(value);
}

int envInt(const char* key, int fallback) {
    try {
        return std::stoi(env(key, std::to_string(fallback)));
    } catch (...) {
        return fallback;
    }
}

std::string tinyDemoReply(const xp::ai::ChatRequest& input) {
    const std::vector<std::string> suggestions = {
        "Try the /api/echo endpoint with your own JSON body.",
        "Use /api/stats to see how many sandbox requests this process handled.",
        "This demo model is intentionally tiny. Swap this function with llama.cpp, ONNX Runtime, or your own C++ inference code.",
        "Xpress++ is useful when your native C++ code needs a clean HTTP API.",
    };

    std::size_t score = 0;
    for (unsigned char c : input.message) {
        score = (score * 131) + c;
    }

    std::ostringstream reply;
    reply << "You said: \"" << input.message << "\". "
          << suggestions[score % suggestions.size()];
    return reply.str();
}

Json::Value runtimeInfo(std::uint64_t requests, const std::chrono::steady_clock::time_point& started_at) {
    const auto uptime = std::chrono::steady_clock::now() - started_at;
    Json::Value info(Json::objectValue);
    info["framework"] = "Xpress++";
    info["mode"] = "public-sandbox";
    info["requestsHandled"] = static_cast<Json::UInt64>(requests);
    info["uptimeSeconds"] = static_cast<Json::Int64>(
        std::chrono::duration_cast<std::chrono::seconds>(uptime).count());
    info["workerThreads"] = static_cast<Json::UInt64>(std::thread::hardware_concurrency());
    return info;
}

xp::Task<xp::ai::ChatResponse> callLocalLlm(const xp::ai::ChatRequest& input,
                                            const std::string& base_url,
                                            const std::string& api_path,
                                            const std::string& model,
                                            double timeout_seconds) {
    Json::Value upstream_body(Json::objectValue);
    upstream_body["model"] = model;
    upstream_body["temperature"] = 0.7;
    upstream_body["max_tokens"] = 160;
    upstream_body["messages"] = Json::arrayValue;

    Json::Value system(Json::objectValue);
    system["role"] = "system";
    system["content"] =
        "You are the tiny public demo model for Xpress++. Keep answers short, "
        "practical, and honest. Mention that Xpress++ is useful for exposing "
        "native C++ services and local AI workloads as HTTP APIs when relevant.";
    upstream_body["messages"].append(system);

    Json::Value user(Json::objectValue);
    user["role"] = "user";
    user["content"] = input.message;
    upstream_body["messages"].append(user);

    auto client = drogon::HttpClient::newHttpClient(base_url);
    auto upstream_req = drogon::HttpRequest::newHttpJsonRequest(upstream_body);
    upstream_req->setMethod(drogon::Post);
    upstream_req->setPath(api_path);

    auto upstream_res = co_await client->sendRequestCoro(upstream_req, timeout_seconds);
    if (!upstream_res) {
        throw std::runtime_error("Local LLM server did not return a response.");
    }
    const auto status = static_cast<int>(upstream_res->statusCode());
    if (status < 200 || status >= 300) {
        throw std::runtime_error("Local LLM server returned HTTP " + std::to_string(status) + ".");
    }

    const auto& json = upstream_res->getJsonObject();
    if (!json || !json->isObject() || !(*json)["choices"].isArray() || (*json)["choices"].empty()) {
        throw std::runtime_error("Local LLM server returned an unexpected response shape.");
    }

    std::string content;
    const auto& first = (*json)["choices"][0];
    if (first.isMember("message") && first["message"].isObject()) {
        content = first["message"]["content"].asString();
    } else if (first.isMember("text")) {
        content = first["text"].asString();
    }
    if (content.empty()) {
        throw std::runtime_error("Local LLM response did not include generated text.");
    }

    xp::ai::ChatResponse output;
    output.reply = content;
    output.model = model;
    output.metadata["runtime"] = "llama.cpp";
    output.metadata["upstream"] = base_url;
    co_return output;
}

} // namespace

int main() {
    xp::loadEnv();

    xp::App app;
    xp::AppConfig config = app.config();
    config.debug = false;
    config.trustProxy = true;
    config.showBanner = true;
    app.configure(config);

    const auto started_at = std::chrono::steady_clock::now();
    std::atomic<std::uint64_t> requests{0};
    const std::string llm_base_url = env("LLM_BASE_URL", "");
    const std::string llm_api_path = env("LLM_API_PATH", "/v1/chat/completions");
    const std::string llm_model = env("LLM_MODEL", "xpresspp-qwen2.5-0.5b");
    const double llm_timeout = static_cast<double>(envInt("LLM_TIMEOUT_SECONDS", 45));

    app.use(xp::requestId());
    app.use(xp::logger());
    app.use(xp::securityHeaders());
    app.use(xp::cors());

    xp::RateLimitOptions rate;
    rate.max = envInt("SANDBOX_RATE_LIMIT", 120);
    rate.windowMs = 60000;
    rate.message = "Sandbox rate limit reached. Please wait a moment and try again.";
    app.use(xp::rateLimit(rate));

    app.use([&](xp::Request& req, xp::Response& res, xp::Next next) {
        requests.fetch_add(1, std::memory_order_relaxed);
        next();
    });

    app.get("/", [](xp::Request&, xp::Response& res) {
        res.ok({
            {"name", "Xpress++ public sandbox"},
            {"message", "Try /health, /api/echo, /api/stats, or /api/ai/chat"},
            {"endpoints", xp::array<std::string>({
                "GET /health",
                "GET /api/stats",
                "POST /api/echo",
                "POST /api/ai/chat",
                "GET /api/llm/status",
                "POST /api/llm/chat"
            })}
        });
    });

    app.health("/health");

    app.get("/api/stats", [&](xp::Request&, xp::Response& res) {
        res.ok(runtimeInfo(requests.load(std::memory_order_relaxed), started_at));
    });

    app.get("/api/llm/status", [=](xp::Request&, xp::Response& res) {
        res.ok({
            {"enabled", !llm_base_url.empty()},
            {"baseUrl", llm_base_url.empty() ? Json::Value(Json::nullValue) : Json::Value(llm_base_url)},
            {"apiPath", llm_api_path},
            {"model", llm_model},
            {"hint", llm_base_url.empty()
                ? "Run scripts/run_qwen_llm.sh, then start the sandbox with LLM_BASE_URL=http://127.0.0.1:8081."
                : "Local LLM proxy is configured."}
        });
    });

    app.post("/api/echo", [](xp::Request& req, xp::Response& res) {
        Json::Value payload(Json::objectValue);
        payload["received"] = req.json();
        payload["method"] = req.method();
        payload["path"] = req.path();
        payload["requestId"] = req.header("x-request-id");
        res.ok(payload);
    });

    xp::ai::ChatOptions chat_options;
    chat_options.defaultModel = "xpresspp-tiny-demo";
    app.post("/api/ai/chat", xp::ai::chat([](const xp::ai::ChatRequest& input) {
        xp::ai::ChatResponse output;
        output.reply = tinyDemoReply(input);
        output.model = "xpresspp-tiny-demo";
        output.metadata["runtime"] = "built-in-demo";
        output.metadata["note"] = "Replace tinyDemoReply() with a real C++ model call.";
        return output;
    }, chat_options));

    app.post("/api/llm/chat", [=](xp::Request& req, xp::Response& res) async {
        if (llm_base_url.empty()) {
            res.status(503).json({
                {"status", "error"},
                {"message", "Real LLM mode is not configured on this sandbox."},
                {"hint", "Run scripts/run_qwen_llm.sh, then start the sandbox with LLM_BASE_URL=http://127.0.0.1:8081."}
            });
            co_return;
        }

        const auto body = req.json();
        if (!body.isObject() || !body.isMember("message") || !body["message"].isString() ||
            body["message"].asString().empty()) {
            res.badRequest("LLM chat requires a non-empty 'message' field.");
            co_return;
        }

        xp::ai::ChatRequest input;
        input.message = body["message"].asString();
        input.model = llm_model;
        input.body = body;

        const auto started = std::chrono::steady_clock::now();
        auto output = co_await callLocalLlm(input, llm_base_url, llm_api_path, llm_model, llm_timeout);
        Json::Value payload(Json::objectValue);
        payload["reply"] = output.reply;
        payload["model"] = output.model;
        payload["metadata"] = output.metadata;
        payload["durationMs"] = static_cast<Json::Int64>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count());
        res.ok(payload);
        co_return;
    });

    const int port = envInt("PORT", 8080);
    app.listen("0.0.0.0", port);
    return 0;
}
