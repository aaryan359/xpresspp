#pragma once

#include <drogon/drogon.h>
#include <json/json.h>

#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>

namespace xp {

class Response {
private:
    drogon::HttpResponsePtr native_response_;
    bool sent_ = false;
    int status_code_ = 200;

    static std::string mimeTypeFor(const std::filesystem::path& path) {
        const auto ext = path.extension().string();
        if (ext == ".html" || ext == ".htm") return "text/html; charset=utf-8";
        if (ext == ".css") return "text/css; charset=utf-8";
        if (ext == ".js") return "application/javascript; charset=utf-8";
        if (ext == ".json") return "application/json; charset=utf-8";
        if (ext == ".png") return "image/png";
        if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
        if (ext == ".gif") return "image/gif";
        if (ext == ".svg") return "image/svg+xml";
        if (ext == ".txt") return "text/plain; charset=utf-8";
        if (ext == ".pdf") return "application/pdf";
        return "application/octet-stream";
    }

public:
    Response() : native_response_(drogon::HttpResponse::newHttpResponse()) {
        native_response_->setStatusCode(drogon::k200OK);
    }

    Response& status(int code) {
        status_code_ = code;
        native_response_->setStatusCode(static_cast<drogon::HttpStatusCode>(code));
        return *this;
    }

    int statusCode() const {
        return status_code_;
    }

    Response& header(const std::string& key, const std::string& value) {
        native_response_->addHeader(key, value);
        return *this;
    }

    Response& type(const std::string& content_type) {
        native_response_->addHeader("Content-Type", content_type);
        return *this;
    }

    Response& send(const std::string& text) {
        native_response_->setBody(text);
        native_response_->setContentTypeCode(drogon::CT_TEXT_PLAIN);
        sent_ = true;
        return *this;
    }

    Response& text(const std::string& value) {
        return send(value);
    }

    Response& html(const std::string& markup) {
        native_response_->setBody(markup);
        native_response_->addHeader("Content-Type", "text/html; charset=utf-8");
        sent_ = true;
        return *this;
    }

    Response& json(const Json::Value& value) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        native_response_->setBody(Json::writeString(builder, value));
        native_response_->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        sent_ = true;
        return *this;
    }

    Response& json(std::initializer_list<std::pair<std::string, Json::Value>> items) {
        Json::Value root;
        for (const auto& item : items) {
            root[item.first] = item.second;
        }
        return json(root);
    }

    Response& json(const std::unordered_map<std::string, std::string>& items) {
        Json::Value root;
        for (const auto& item : items) {
            root[item.first] = item.second;
        }
        return json(root);
    }

    Response& redirect(const std::string& url, int code = 302) {
        status(code);
        native_response_->addHeader("Location", url);
        sent_ = true;
        return *this;
    }

    Response& cookie(const std::string& key,
                     const std::string& value,
                     int max_age_seconds = 0,
                     bool http_only = true,
                     bool secure = false,
                     const std::string& same_site = "Lax") {
        std::ostringstream cookie;
        cookie << key << "=" << value << "; Path=/";
        if (max_age_seconds > 0) {
            cookie << "; Max-Age=" << max_age_seconds;
        }
        if (http_only) {
            cookie << "; HttpOnly";
        }
        if (secure) {
            cookie << "; Secure";
        }
        if (!same_site.empty()) {
            cookie << "; SameSite=" << same_site;
        }
        native_response_->addHeader("Set-Cookie", cookie.str());
        return *this;
    }

    Response& clearCookie(const std::string& key) {
        native_response_->addHeader("Set-Cookie", key + "=; Path=/; Max-Age=0");
        return *this;
    }

    Response& file(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
            status(404).json({
                {"status", "error"},
                {"message", "File not found"}
            });
            return *this;
        }

        native_response_ = drogon::HttpResponse::newFileResponse(path.string());
        sent_ = true;
        return *this;
    }

    Response& download(const std::filesystem::path& path, const std::string& filename = "") {
        if (!std::filesystem::exists(path) || !std::filesystem::is_regular_file(path)) {
            status(404).json({
                {"status", "error"},
                {"message", "File not found"}
            });
            return *this;
        }

        const std::string suggested = filename.empty() ? path.filename().string() : filename;
        native_response_ = drogon::HttpResponse::newFileResponse(path.string(), suggested);
        sent_ = true;
        return *this;
    }

    Response& location(const std::string& url) {
        native_response_->addHeader("Location", url);
        return *this;
    }

    Response& noContent() {
        native_response_->setBody("");
        status(204);
        sent_ = true;
        return *this;
    }

    Response& created(const Json::Value& value, const std::string& url = "") {
        status(201);
        if (!url.empty()) {
            location(url);
        }
        return json(value);
    }

    Response& created(std::initializer_list<std::pair<std::string, Json::Value>> items,
                      const std::string& url = "") {
        status(201);
        if (!url.empty()) {
            location(url);
        }
        return json(items);
    }

    Response& badRequest(const std::string& message = "Bad request") {
        return status(400).json({{"status", "error"}, {"message", message}});
    }

    Response& unauthorized(const std::string& message = "Unauthorized") {
        return status(401).json({{"status", "error"}, {"message", message}});
    }

    Response& forbidden(const std::string& message = "Forbidden") {
        return status(403).json({{"status", "error"}, {"message", message}});
    }

    Response& notFound(const std::string& message = "Not found") {
        return status(404).json({{"status", "error"}, {"message", message}});
    }

    Response& serverError(const std::string& message = "Internal server error") {
        return status(500).json({{"status", "error"}, {"message", message}});
    }

    bool sent() const {
        return sent_;
    }

    drogon::HttpResponsePtr native() const {
        return native_response_;
    }

    drogon::HttpResponsePtr get_native_response() const {
        return native_response_;
    }
};

} // namespace xp
