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

inline Json::Value rowToJson(const drogon::orm::Result& res, const drogon::orm::Row& row) {
    Json::Value val(Json::objectValue);
    for (std::size_t i = 0; i < row.size(); ++i) {
        const auto& field = row[i];
        std::string colName = res.columnName(i);
        if (field.isNull()) {
            val[colName] = Json::Value();
        } else {
            try {
                val[colName] = field.as<std::string>();
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

} // namespace xp
