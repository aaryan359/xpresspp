#pragma once

#include <string>
#include <vector>
#include <functional>
#include <json/json.h>

namespace xp {

class ValidationRule {
public:
    using CheckFunc = std::function<std::string(const std::string& field_name, const Json::Value& value)>;

private:
    std::vector<CheckFunc> checks_;
    bool is_required_ = false;
    std::string required_msg_;

public:
    ValidationRule() = default;

    ValidationRule& required(std::string custom_msg = "") {
        is_required_ = true;
        required_msg_ = std::move(custom_msg);
        return *this;
    }

    bool isRequired() const { return is_required_; }
    const std::string& requiredMsg() const { return required_msg_; }
    const std::vector<CheckFunc>& checks() const { return checks_; }

    ValidationRule& min(int len, std::string custom_msg = "") {
        checks_.push_back([len, msg = std::move(custom_msg)](const std::string& name, const Json::Value& val) -> std::string {
            if (!val.isString()) {
                return "Field '" + name + "' must be a string";
            }
            if (static_cast<int>(val.asString().length()) < len) {
                return !msg.empty() ? msg : ("Field '" + name + "' must be at least " + std::to_string(len) + " characters");
            }
            return "";
        });
        return *this;
    }

    ValidationRule& max(int len, std::string custom_msg = "") {
        checks_.push_back([len, msg = std::move(custom_msg)](const std::string& name, const Json::Value& val) -> std::string {
            if (!val.isString()) {
                return "Field '" + name + "' must be a string";
            }
            if (static_cast<int>(val.asString().length()) > len) {
                return !msg.empty() ? msg : ("Field '" + name + "' must be at most " + std::to_string(len) + " characters");
            }
            return "";
        });
        return *this;
    }

    ValidationRule& email(std::string custom_msg = "") {
        checks_.push_back([msg = std::move(custom_msg)](const std::string& name, const Json::Value& val) -> std::string {
            if (!val.isString()) {
                return "Field '" + name + "' must be a string";
            }
            const std::string s = val.asString();
            if (s.find('@') == std::string::npos || s.find('.') == std::string::npos) {
                return !msg.empty() ? msg : ("Field '" + name + "' must be a valid email address");
            }
            return "";
        });
        return *this;
    }

    ValidationRule& custom(std::function<bool(const Json::Value&)> predicate, std::string custom_msg) {
        checks_.push_back([predicate, msg = std::move(custom_msg)](const std::string& name, const Json::Value& val) -> std::string {
            if (!predicate(val)) {
                return msg;
            }
            return "";
        });
        return *this;
    }

    ValidationRule& unique(std::string table, std::string col, std::string custom_msg = "") {
        checks_.push_back([table, col, msg = std::move(custom_msg)](const std::string& name, const Json::Value& val) -> std::string {
            // DB checks can be registered here in the future
            return "";
        });
        return *this;
    }
};

inline ValidationRule string() {
    ValidationRule r;
    r.custom([](const Json::Value& val) { return val.isString(); }, "Must be a string");
    return r;
}

inline ValidationRule number() {
    ValidationRule r;
    r.custom([](const Json::Value& val) { return val.isNumeric(); }, "Must be a number");
    return r;
}

inline ValidationRule boolean() {
    ValidationRule r;
    r.custom([](const Json::Value& val) { return val.isBool(); }, "Must be a boolean");
    return r;
}

inline ValidationRule any() {
    return ValidationRule();
}

struct ValidationError {
    std::string message;

    operator bool() const { return !message.empty(); }
    operator Json::Value() const {
        Json::Value val;
        val["error"] = message;
        return val;
    }
    operator std::string() const {
        return message;
    }
};

struct ValidationResult {
    Json::Value body;
    ValidationError error;
};

} // namespace xp
