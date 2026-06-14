#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <string>

namespace xp {

struct DbConfig {
    std::string driver = "sqlite3"; // "postgresql", "mysql", "sqlite3"
    std::string host = "127.0.0.1";
    int port = 0;
    std::string database;
    std::string username;
    std::string password;
    std::size_t connection_number = 1;
    std::string name = "default";
};

inline drogon::orm::DbClientPtr db(const std::string& name = "default") {
    return drogon::app().getDbClient(name);
}

template <typename... Args>
inline auto query(const std::string& sql, Args&&... args) {
    return db()->execSqlCoro(sql, std::forward<Args>(args)...);
}

template <typename... Args>
inline auto queryClient(const std::string& clientName, const std::string& sql, Args&&... args) {
    return db(clientName)->execSqlCoro(sql, std::forward<Args>(args)...);
}

inline Json::Value parseFieldToValue(const std::string& str) {
    if (str.empty()) {
        return Json::Value("");
    }
    
    // Check if it is a boolean
    if (str == "t" || str == "true" || str == "TRUE") {
        return Json::Value(true);
    }
    if (str == "f" || str == "false" || str == "FALSE") {
        return Json::Value(false);
    }
    
    // Check if it is a JSON object or array
    if ((str.front() == '{' && str.back() == '}') || (str.front() == '[' && str.back() == ']')) {
        Json::Value nested;
        Json::Reader reader;
        if (reader.parse(str, nested)) {
            return nested;
        }
    }
    
    // Check if it is numeric (integer or float)
    bool has_decimal = false;
    bool has_digits = false;
    bool is_numeric = true;
    
    std::size_t start = 0;
    if (str[0] == '-' || str[0] == '+') {
        start = 1;
    }
    
    if (start == str.length()) {
        is_numeric = false;
    }
    
    for (std::size_t i = start; i < str.length(); ++i) {
        char c = str[i];
        if (c == '.') {
            if (has_decimal) {
                is_numeric = false;
                break;
            }
            has_decimal = true;
        } else if (std::isdigit(c)) {
            has_digits = true;
        } else {
            is_numeric = false;
            break;
        }
    }
    
    if (is_numeric && has_digits) {
        try {
            if (has_decimal) {
                return Json::Value(std::stod(str));
            } else {
                return Json::Value(static_cast<Json::Int64>(std::stoll(str)));
            }
        } catch (...) {
            // fallback
        }
    }
    
    return Json::Value(str);
}

inline Json::Value rowToJson(const drogon::orm::Result& res, const drogon::orm::Row& row) {
    Json::Value val(Json::objectValue);
    for (std::size_t i = 0; i < row.size(); ++i) {
        const auto& field = row[i];
        std::string colName = res.columnName(i);
        if (field.isNull()) {
            val[colName] = Json::Value();
        } else {
            try {
                val[colName] = parseFieldToValue(field.as<std::string>());
            } catch (...) {
                val[colName] = Json::Value();
            }
        }
    }
    return val;
}

inline Json::Value resultToJson(const drogon::orm::Result& res) {
    Json::Value arr(Json::arrayValue);
    for (const auto& row : res) {
        arr.append(rowToJson(res, row));
    }
    return arr;
}

template <typename... Args>
inline drogon::Task<Json::Value> queryJson(const std::string& sql, Args&&... args) {
    auto result = co_await query(sql, std::forward<Args>(args)...);
    co_return resultToJson(result);
}

template <typename... Args>
inline drogon::Task<Json::Value> queryOneJson(const std::string& sql, Args&&... args) {
    auto result = co_await query(sql, std::forward<Args>(args)...);
    if (result.empty()) {
        co_return Json::Value();
    }
    co_return rowToJson(result, result[0]);
}

} // namespace xp
