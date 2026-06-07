#pragma once

#include "../router.h"

#include <functional>
#include <string>
#include <utility>
#include <initializer_list>
#include <vector>
#include <json/json.h>

namespace xp {

using Validator = std::function<std::string(Request&)>;

inline Middleware validate(Validator validator) {
    return [validator = std::move(validator)](Request& req, Response& res, Next next) {
        const auto error = validator(req);
        if (!error.empty()) {
            res.badRequest(error);
            return;
        }
        next();
    };
}

enum class Type {
    String,
    Number,
    Boolean,
    Object,
    Array,
    Integer
};

struct FieldRule {
    std::string name;
    Type type;
    bool required = true;
};

inline Middleware validate(std::initializer_list<FieldRule> rules) {
    return [fields = std::vector<FieldRule>(rules)](Request& req, Response& res, Next next) {
        if (req.method() == "POST" || req.method() == "PUT" || req.method() == "PATCH") {
            if (!req.isJson()) {
                res.badRequest("Content-Type must be application/json");
                return;
            }
            
            const auto body = req.json();
            for (const auto& rule : fields) {
                if (!body.isMember(rule.name) || body[rule.name].isNull()) {
                    if (rule.required) {
                        res.badRequest("Field '" + rule.name + "' is required");
                        return;
                    }
                    continue;
                }
                
                const auto& val = body[rule.name];
                bool typeOk = false;
                std::string expectedType;
                
                switch (rule.type) {
                    case Type::String:
                        typeOk = val.isString();
                        expectedType = "string";
                        break;
                    case Type::Number:
                        typeOk = val.isNumeric();
                        expectedType = "number";
                        break;
                    case Type::Boolean:
                        typeOk = val.isBool();
                        expectedType = "boolean";
                        break;
                    case Type::Object:
                        typeOk = val.isObject();
                        expectedType = "object";
                        break;
                    case Type::Array:
                        typeOk = val.isArray();
                        expectedType = "array";
                        break;
                    case Type::Integer:
                        typeOk = val.isInt() || val.isInt64() || val.isUInt() || val.isUInt64();
                        expectedType = "integer";
                        break;
                }
                
                if (!typeOk) {
                    res.badRequest("Field '" + rule.name + "' must be a " + expectedType);
                    return;
                }
            }
        }
        next();
    };
}

} // namespace xp
