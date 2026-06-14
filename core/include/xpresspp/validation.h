#pragma once

#include <string>
#include <vector>
#include <json/json.h>

namespace xp {

enum RuleFlags {
    Required = 1 << 0,
    Email = 1 << 1
};

struct ValidationRule {
    int flags = 0;
    int min_length = -1;
    int max_length = -1;

    ValidationRule() = default;
    ValidationRule(RuleFlags f) : flags(f) {}

    ValidationRule operator|(RuleFlags f) const {
        ValidationRule r = *this;
        r.flags |= f;
        return r;
    }

    ValidationRule operator|(const ValidationRule& other) const {
        ValidationRule r = *this;
        r.flags |= other.flags;
        if (other.min_length != -1) r.min_length = other.min_length;
        if (other.max_length != -1) r.max_length = other.max_length;
        return r;
    }
};

inline ValidationRule operator|(RuleFlags a, RuleFlags b) {
    ValidationRule r;
    r.flags = static_cast<int>(a) | static_cast<int>(b);
    return r;
}

inline ValidationRule operator|(RuleFlags a, const ValidationRule& b) {
    ValidationRule r = b;
    r.flags |= a;
    return r;
}

inline ValidationRule MinLength(int len) {
    ValidationRule r;
    r.min_length = len;
    return r;
}

inline ValidationRule MaxLength(int len) {
    ValidationRule r;
    r.max_length = len;
    return r;
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
