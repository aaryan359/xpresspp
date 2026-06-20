#pragma once
#include "errors.h"
#include <json/json.h>
#include <initializer_list>
#include <utility>


namespace xp {

struct json_obj : public Json::Value {
    json_obj() : Json::Value(Json::objectValue) {}
    json_obj(std::initializer_list<std::pair<std::string, Json::Value>> items) : Json::Value(Json::objectValue) {
        for (const auto& pair : items) {
            (*this)[pair.first] = pair.second;
        }
    }
};

struct json_arr : public Json::Value {
    json_arr() : Json::Value(Json::arrayValue) {}
    json_arr(std::initializer_list<Json::Value> items) : Json::Value(Json::arrayValue) {
        for (const auto& item : items) {
            this->append(item);
        }
    }
};

using obj = json_obj;
using arr = json_arr;

} // namespace xp
