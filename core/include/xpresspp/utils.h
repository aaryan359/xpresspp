#pragma once
#include "errors.h"
#include <json/json.h>
#include <initializer_list>
#include <utility>
#include <string>

namespace xp {

class var : public Json::Value {
public:
    using Json::Value::Value;

    var() : Json::Value(Json::nullValue) {}
    var(const Json::Value& other) : Json::Value(other) {}
    var(Json::Value&& other) : Json::Value(std::move(other)) {}

    // Implicit conversion to std::string
    operator std::string() const {
        if (isNull()) return "";
        if (isString()) return asString();
        // Fallback: output as styled/unstyled string representation
        return asString();
    }

    // Implicit conversion to int
    operator int() const {
        if (isNull()) return 0;
        if (isInt()) return asInt();
        if (isDouble()) return static_cast<int>(asDouble());
        if (isString()) {
            try { return std::stoi(asString()); } catch (...) { return 0; }
        }
        return 0;
    }

    // Implicit conversion to double
    operator double() const {
        if (isNull()) return 0.0;
        if (isDouble()) return asDouble();
        if (isInt()) return static_cast<double>(asInt());
        if (isString()) {
            try { return std::stod(asString()); } catch (...) { return 0.0; }
        }
        return 0.0;
    }

    // Implicit conversion to bool
    operator bool() const {
        if (isNull()) return false;
        if (isBool()) return asBool();
        if (isInt()) return asInt() != 0;
        if (isString()) {
            std::string s = asString();
            return s == "true" || s == "1" || s == "yes";
        }
        return false;
    }

    // Overloaded const operator[] to return var by value
    var operator[](const std::string& key) const {
        return Json::Value::operator[](key);
    }
    var operator[](const char* key) const {
        return Json::Value::operator[](key);
    }
    var operator[](int index) const {
        return Json::Value::operator[](index);
    }
    var operator[](unsigned int index) const {
        return Json::Value::operator[](index);
    }

    // Overloaded non-const operator[] to return var&
    var& operator[](const std::string& key) {
        return static_cast<var&>(Json::Value::operator[](key));
    }
    var& operator[](const char* key) {
        return static_cast<var&>(Json::Value::operator[](key));
    }
    var& operator[](int index) {
        return static_cast<var&>(Json::Value::operator[](index));
    }
    var& operator[](unsigned int index) {
        return static_cast<var&>(Json::Value::operator[](index));
    }
};

struct json_obj : public var {
    json_obj() : var(Json::objectValue) {}
    json_obj(std::initializer_list<std::pair<std::string, var>> items) : var(Json::objectValue) {
        for (const auto& pair : items) {
            (*this)[pair.first] = pair.second;
        }
    }
};

struct json_arr : public var {
    json_arr() : var(Json::arrayValue) {}
    json_arr(std::initializer_list<var> items) : var(Json::arrayValue) {
        for (const auto& item : items) {
            this->append(item);
        }
    }
};

using obj = json_obj;
using arr = json_arr;

} // namespace xp
