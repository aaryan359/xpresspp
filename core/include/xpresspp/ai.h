#pragma once

#include "errors.h"
#include "request.h"
#include "response.h"
#include "router.h"

#include <chrono>
#include <concepts>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

namespace xp::ai {

struct ChatRequest {
    std::string message;
    std::string model = "local";
    Json::Value body = Json::objectValue;
};

struct ChatResponse {
    std::string reply;
    std::string model = "local";
    Json::Value metadata = Json::objectValue;
};

struct ChatOptions {
    std::string messageField = "message";
    std::string fallbackMessageField = "prompt";
    std::string responseField = "reply";
    std::string defaultModel = "local";
    bool includeDuration = true;
};

namespace detail {

template <typename T>
inline Json::Value responseToJson(T&& value, std::string_view response_field, std::string_view model) {
    using Value = std::remove_cvref_t<T>;

    Json::Value payload(Json::objectValue);
    if constexpr (std::is_same_v<Value, ChatResponse>) {
        payload[std::string(response_field)] = value.reply;
        payload["model"] = value.model.empty() ? std::string(model) : value.model;
        if (!value.metadata.isNull() && !value.metadata.empty()) {
            payload["metadata"] = value.metadata;
        }
    } else if constexpr (std::is_same_v<Value, Json::Value>) {
        payload = std::forward<T>(value);
        if (!payload.isMember("model")) {
            payload["model"] = std::string(model);
        }
    } else if constexpr (std::is_convertible_v<Value, std::string>) {
        payload[std::string(response_field)] = std::string(std::forward<T>(value));
        payload["model"] = std::string(model);
    } else {
        static_assert(std::is_same_v<Value, ChatResponse> ||
                      std::is_same_v<Value, Json::Value> ||
                      std::is_convertible_v<Value, std::string>,
                      "xp::ai::chat callback must return std::string, Json::Value, or xp::ai::ChatResponse.");
    }
    return payload;
}

inline ChatRequest parseChatRequest(Request& req, const ChatOptions& options) {
    const auto body = req.json();
    if (!body.isObject()) {
        throw BadRequestError("AI chat request body must be a JSON object.");
    }

    const bool has_message = body.isMember(options.messageField) &&
                             body[options.messageField].isString() &&
                             !body[options.messageField].asString().empty();
    const bool has_fallback = !options.fallbackMessageField.empty() &&
                              body.isMember(options.fallbackMessageField) &&
                              body[options.fallbackMessageField].isString() &&
                              !body[options.fallbackMessageField].asString().empty();

    if (!has_message && !has_fallback) {
        throw BadRequestError("AI chat request requires a non-empty '" +
                              options.messageField + "' field.");
    }

    ChatRequest input;
    input.message = has_message
        ? body[options.messageField].asString()
        : body[options.fallbackMessageField].asString();
    input.model = body.isMember("model") && body["model"].isString()
        ? body["model"].asString()
        : options.defaultModel;
    input.body = body;
    return input;
}

} // namespace detail

template <typename Generator>
SyncHandler chat(Generator&& generator, ChatOptions options = {}) {
    auto target = std::forward<Generator>(generator);
    return
        [target = std::move(target), options = std::move(options)](Request& req, Response& res) mutable {
            const auto started = std::chrono::steady_clock::now();
            auto input = detail::parseChatRequest(req, options);

            Json::Value payload;
            if constexpr (std::is_invocable_v<Generator&, const ChatRequest&>) {
                payload = detail::responseToJson(target(input), options.responseField, input.model);
            } else if constexpr (std::is_invocable_v<Generator&, std::string>) {
                payload = detail::responseToJson(target(input.message), options.responseField, input.model);
            } else {
                static_assert(std::is_invocable_v<Generator&, const ChatRequest&> ||
                              std::is_invocable_v<Generator&, std::string>,
                              "xp::ai::chat callback must accept xp::ai::ChatRequest or std::string.");
            }

            if (options.includeDuration) {
                const auto elapsed = std::chrono::steady_clock::now() - started;
                payload["durationMs"] = static_cast<Json::Int64>(
                    std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
            }

            res.ok(payload);
        };
}

template <typename Generator>
SyncHandler completion(Generator&& generator, ChatOptions options = {}) {
    if (options.responseField == "reply") {
        options.responseField = "completion";
    }
    return chat(std::forward<Generator>(generator), std::move(options));
}

} // namespace xp::ai
