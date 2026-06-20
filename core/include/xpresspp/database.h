#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <variant>
#include <type_traits>

#if __has_include(<mongocxx/client.hpp>)
#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/uri.hpp>
#include <memory>
#endif

namespace xp {

struct DbConfig {
    std::string driver = "sqlite3"; // "postgresql", "mysql", "sqlite3", "mongodb"
    std::string host = "127.0.0.1";
    int port = 0;
    std::string database;
    std::string username;
    std::string password;
    std::size_t connection_number = 1;
    std::string name = "default";
};

#if __has_include(<mongocxx/client.hpp>)
class MongoClientManager {
private:
    std::unique_ptr<mongocxx::instance> instance_;
    std::unique_ptr<mongocxx::client> client_;
    std::string db_name_;

    MongoClientManager() = default;

public:
    static MongoClientManager& get() {
        static MongoClientManager manager;
        return manager;
    }

    void connect(const std::string& uri_str, const std::string& db_name) {
        if (!instance_) {
            instance_ = std::make_unique<mongocxx::instance>();
        }
        client_ = std::make_unique<mongocxx::client>(mongocxx::uri(uri_str));
        db_name_ = db_name;
    }

    mongocxx::database db() {
        if (!client_) {
            throw std::runtime_error("MongoDB client is not connected. Call database(\"mongodb://...\") first.");
        }
        return (*client_)[db_name_];
    }
};
#endif

inline drogon::orm::DbClientPtr db(const std::string& name = "default") {
    return drogon::app().getDbClient(name);
}

inline std::string& currentDriver() {
    static std::string driver = "sqlite3";
    return driver;
}

// Unified Query Parameter type for SQL databases
using QueryParam = std::variant<std::nullptr_t, bool, int32_t, int64_t, double, std::string>;

inline QueryParam jsonToQueryParam(const Json::Value& val) {
    if (val.isNull()) return nullptr;
    if (val.isBool()) return val.asBool();
    if (val.isInt()) return static_cast<int64_t>(val.asInt());
    if (val.isInt64()) return val.asInt64();
    if (val.isUInt()) return static_cast<int64_t>(val.asUInt());
    if (val.isUInt64()) return static_cast<int64_t>(val.asUInt64());
    if (val.isDouble()) return val.asDouble();
    return val.asString();
}

inline std::string getPlaceholder(const std::string& driver, int index) {
    if (driver == "postgresql") {
        return "$" + std::to_string(index);
    }
    return "?";
}

// Executes a parameterized SQL query asynchronously with parameter bindings
inline drogon::Task<drogon::orm::Result> executeParameterized(
    drogon::orm::DbClientPtr client, 
    const std::string& sql, 
    const std::vector<QueryParam>& params
) {
    auto binder = *client << sql;
    for (const auto& param : params) {
        std::visit([&binder](auto&& arg) {
            binder << arg;
        }, param);
    }
    co_return co_await drogon::orm::internal::SqlAwaiter(std::move(binder));
}

inline drogon::Task<drogon::orm::Result> executeParameterized(
    const std::string& sql, 
    const std::vector<QueryParam>& params
) {
    co_return co_await executeParameterized(db(), sql, params);
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

// Parameterized Query Json Helpers
inline drogon::Task<xp::var> queryJson(const std::string& sql, const std::vector<QueryParam>& params = {}) {
    auto result = co_await executeParameterized(db(), sql, params);
    co_return resultToJson(result);
}

inline drogon::Task<xp::var> queryOneJson(const std::string& sql, const std::vector<QueryParam>& params = {}) {
    auto result = co_await executeParameterized(db(), sql, params);
    if (result.empty()) {
        co_return xp::var();
    }
    co_return rowToJson(result, result[0]);
}

} // namespace xp
