#pragma once

#include <drogon/drogon.h>
#include <drogon/MultiPart.h>
#include <json/json.h>

#include <algorithm>
#include <any>
#include <cctype>
#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

#include "errors.h"
#include "validation.h"

namespace xp {

class Request {
private:
    drogon::HttpRequestPtr native_request_;
    std::unordered_map<std::string, std::string> params_;
    bool trust_proxy_ = false;

    ValidationResult validateData(const xp::var& data, std::initializer_list<std::pair<std::string, ValidationRule>> rules) const {
        ValidationResult result;
        for (const auto& pair : rules) {
            const auto& name = pair.first;
            const auto& rule = pair.second;

            if (!data.isMember(name) || data[name].isNull()) {
                if (rule.isRequired()) {
                    result.error.message = !rule.requiredMsg().empty() ? rule.requiredMsg() : ("Field '" + name + "' is required");
                    return result;
                }
                continue;
            }

            const auto& val = data[name];
            for (const auto& check : rule.checks()) {
                std::string err_msg = check(name, val);
                if (!err_msg.empty()) {
                    result.error.message = err_msg;
                    return result;
                }
            }
        }
        result.body = data;
        return result;
    }

public:
    // Request-scoped data store — same concept as Express req.locals.
    // Middleware can store any typed value; route handlers read it back.
    //
    //   middleware:  req.locals["user"] = verifiedUser;
    //   handler:     auto user = req.local<User>("user");
    std::unordered_map<std::string, std::any> locals;

    // Typed getter with default — returns default_val if key absent or wrong type
    template <typename T>
    T local(const std::string& key, T default_val = T{}) const {
        auto it = locals.find(key);
        if (it == locals.end()) return default_val;
        try {
            return std::any_cast<T>(it->second);
        } catch (const std::bad_any_cast&) {
            return default_val;
        }
    }

private:
    static std::string lower(std::string value) {
        std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return value;
    }

    static std::string trim(const std::string& value) {
        const auto begin = value.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) {
            return "";
        }
        const auto end = value.find_last_not_of(" \t\r\n");
        return value.substr(begin, end - begin + 1);
    }

    static int fromHex(char c) {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    }

    static std::string urlDecode(const std::string& value) {
        std::string decoded;
        decoded.reserve(value.size());
        for (std::size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '+' ) {
                decoded.push_back(' ');
            } else if (value[i] == '%' && i + 2 < value.size()) {
                const int hi = fromHex(value[i + 1]);
                const int lo = fromHex(value[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    decoded.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                } else {
                    decoded.push_back(value[i]);
                }
            } else {
                decoded.push_back(value[i]);
            }
        }
        return decoded;
    }

public:
    explicit Request(drogon::HttpRequestPtr req, bool trust_proxy = false)
        : native_request_(std::move(req)), trust_proxy_(trust_proxy) {}

    std::string path() const {
        return native_request_->getPath();
    }

    std::string url() const {
        return native_request_->getPath();
    }

    std::string originalUrl() const {
        return url();
    }

    std::string method() const {
        return native_request_->getMethodString();
    }

    std::string query(const std::string& key) const {
        return native_request_->getParameter(key);
    }

    std::string query(const std::string& key, const std::string& default_value) const {
        const auto value = query(key);
        return value.empty() ? default_value : value;
    }

    std::string query(const std::string& key, const char* default_value) const {
        return query(key, std::string(default_value));
    }

    std::unordered_map<std::string, std::string> query() const {
        std::unordered_map<std::string, std::string> result;
        for (const auto& item : native_request_->getParameters()) {
            result[item.first] = item.second;
        }
        return result;
    }

    std::string body() const {
        const auto body_view = native_request_->getBody();
        return std::string(body_view.data(), body_view.size());
    }

    template <typename T>
    T body() const {
        if constexpr (std::is_same_v<T, std::string>) {
            return body();
        } else if constexpr (std::is_same_v<T, Json::Value>) {
            return json();
        } else {
            static_assert(std::is_same_v<T, std::string> || std::is_same_v<T, Json::Value>,
                          "req.body<T>() currently supports std::string and Json::Value.");
        }
    }

    xp::var json() const {
        Json::CharReaderBuilder builder;
        xp::var root;
        std::string errors;
        const std::string raw_body = body();

        auto reader = std::unique_ptr<Json::CharReader>(builder.newCharReader());
        if (!reader->parse(raw_body.data(), raw_body.data() + raw_body.size(), &root, &errors)) {
            throw std::invalid_argument("Invalid JSON request body: " + errors);
        }

        return root;
    }

    std::unordered_map<std::string, std::string> form() const {
        std::unordered_map<std::string, std::string> result;
        std::stringstream pairs(body());
        std::string pair;
        while (std::getline(pairs, pair, '&')) {
            const auto equals = pair.find('=');
            if (equals == std::string::npos) {
                result[urlDecode(pair)] = "";
                continue;
            }
            result[urlDecode(pair.substr(0, equals))] = urlDecode(pair.substr(equals + 1));
        }
        return result;
    }

    drogon::MultiPartParser multipart() const {
        drogon::MultiPartParser parser;
        if (parser.parse(native_request_) != 0) {
            throw std::invalid_argument("Invalid multipart request body.");
        }
        return parser;
    }

    std::vector<drogon::HttpFile> files() const {
        return multipart().getFiles();
    }

    std::optional<drogon::HttpFile> file(const std::string& key) const {
        auto files_map = multipart().getFilesMap();
        auto it = files_map.find(key);
        if (it == files_map.end()) {
            return std::nullopt;
        }
        return it->second;
    }

    std::string header(const std::string& key) const {
        return native_request_->getHeader(key);
    }

    std::unordered_map<std::string, std::string> headers() const {
        std::unordered_map<std::string, std::string> result;
        for (const auto& item : native_request_->getHeaders()) {
            result[item.first] = item.second;
        }
        return result;
    }

    std::string cookie(const std::string& key) const {
        return native_request_->getCookie(key);
    }

    std::unordered_map<std::string, std::string> cookies() const {
        std::unordered_map<std::string, std::string> result;
        for (const auto& item : native_request_->getCookies()) {
            result[item.first] = item.second;
        }
        return result;
    }

    std::string ip() const {
        if (trust_proxy_) {
            const auto forwarded = header("x-forwarded-for");
            if (!forwarded.empty()) {
                const auto comma = forwarded.find(',');
                return trim(forwarded.substr(0, comma));
            }
            const auto real_ip = header("x-real-ip");
            if (!real_ip.empty()) return trim(real_ip);
        }
        return native_request_->getPeerAddr().toIp();
    }

    std::vector<std::string> ips() const {
        std::vector<std::string> result;
        const auto forwarded = trust_proxy_ ? header("x-forwarded-for") : "";
        if (!forwarded.empty()) {
            std::stringstream parts(forwarded);
            std::string part;
            while (std::getline(parts, part, ',')) {
                result.push_back(trim(part));
            }
        }
        result.push_back(native_request_->getPeerAddr().toIp());
        return result;
    }

    bool trustsProxy() const { return trust_proxy_; }

    static std::string safeUploadName(const std::string& filename) {
        auto name = std::filesystem::path(filename).filename().string();
        std::string safe;
        safe.reserve(name.size());
        for (unsigned char c : name) {
            if (std::isalnum(c) || c == '.' || c == '-' || c == '_') safe.push_back(static_cast<char>(c));
            else safe.push_back('_');
        }
        if (safe.empty() || safe == "." || safe == "..") {
            throw std::invalid_argument("Invalid upload filename.");
        }
        return safe;
    }

    std::string userAgent() const {
        return header("user-agent");
    }

    std::string contentType() const {
        return header("content-type");
    }

    bool is(const std::string& expected) const {
        return lower(contentType()).find(lower(expected)) != std::string::npos;
    }

    bool isJson() const {
        return is("application/json") || is("+json");
    }

    bool isForm() const {
        return is("application/x-www-form-urlencoded") || is("multipart/form-data");
    }

    bool accepts(const std::string& content_type) const {
        const auto accept = lower(header("accept"));
        return accept.empty() || accept == "*/*" || accept.find(lower(content_type)) != std::string::npos;
    }

    std::string param(const std::string& key) const {
        auto it = params_.find(key);
        return it == params_.end() ? "" : it->second;
    }

    std::string param(const std::string& key, const std::string& default_value) const {
        const auto value = param(key);
        return value.empty() ? default_value : value;
    }

    std::string param(const std::string& key, const char* default_value) const {
        return param(key, std::string(default_value));
    }

    std::string param(const std::string& key, bool required) const {
        const auto value = param(key);
        if (required && value.empty()) {
            throw BadRequestError("Missing route parameter: " + key);
        }
        return value;
    }

    const std::unordered_map<std::string, std::string>& params() const {
        return params_;
    }

    void setParam(const std::string& key, const std::string& value) {
        params_[key] = value;
    }

    ValidationResult validate(std::initializer_list<std::pair<std::string, ValidationRule>> rules) const {
        xp::var body_val;
        try {
            body_val = json();
        } catch (const std::exception& e) {
            ValidationResult result;
            result.error.message = std::string("Invalid JSON body: ") + e.what();
            return result;
        }
        return validateData(body_val, rules);
    }

    ValidationResult validateQuery(std::initializer_list<std::pair<std::string, ValidationRule>> rules) const {
        xp::var query_val = xp::obj({});
        for (const auto& [k, v] : query()) {
            query_val[k] = v;
        }
        return validateData(query_val, rules);
    }

    ValidationResult validateParams(std::initializer_list<std::pair<std::string, ValidationRule>> rules) const {
        xp::var params_val = xp::obj({});
        for (const auto& [k, v] : params()) {
            params_val[k] = v;
        }
        return validateData(params_val, rules);
    }

    const drogon::HttpRequestPtr& native() const {
        return native_request_;
    }
};

} // namespace xp
