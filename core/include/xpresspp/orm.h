#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <iostream>
#include "database.h"

namespace xp {

enum class FieldType {
    Serial,
    Integer,
    Text,
    Boolean,
    Double,
    Timestamp
};

inline std::string fieldTypeToSql(FieldType type) {
    switch (type) {
        case FieldType::Serial:    return "SERIAL";
        case FieldType::Integer:   return "BIGINT";
        case FieldType::Text:      return "VARCHAR(255)";
        case FieldType::Boolean:   return "BOOLEAN";
        case FieldType::Double:    return "DOUBLE PRECISION";
        case FieldType::Timestamp: return "TIMESTAMP";
    }
    return "TEXT";
}

enum FieldOption {
    None = 0,
    PrimaryKey = 1 << 0,
    NotNull = 1 << 1,
    Unique = 1 << 2,
    DefaultNow = 1 << 3
};

inline bool hasOption(int options, FieldOption opt) {
    return (options & opt) != 0;
}

struct SchemaField {
    std::string name;
    FieldType type;
    int options = None;
};

using Schema = std::vector<SchemaField>;

// Safe SQL value escaping utility for the query builder
inline std::string escapeValue(const Json::Value& val) {
    if (val.isNull()) {
        return "NULL";
    }
    if (val.isBool()) {
        return val.asBool() ? "TRUE" : "FALSE";
    }
    if (val.isNumeric()) {
        return val.asString();
    }
    
    std::string str = val.asString();
    std::string escaped = "";
    for (char c : str) {
        if (c == '\'') {
            escaped += "''";
        } else {
            escaped += c;
        }
    }
    return "'" + escaped + "'";
}

// Model base class using CRTP (Curiously Recurring Template Pattern)
template <typename Derived>
class Model {
public:
    // Synchronize schema (create table IF NOT EXISTS)
    static drogon::Task<void> sync() {
        std::string tableName = Derived::tableName();
        auto fields = Derived::schema();
        
        std::string sql = "CREATE TABLE IF NOT EXISTS " + tableName + " (";
        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) sql += ", ";
            sql += fields[i].name + " " + fieldTypeToSql(fields[i].type);
            
            if (hasOption(fields[i].options, FieldOption::PrimaryKey)) {
                sql += " PRIMARY KEY";
            }
            if (hasOption(fields[i].options, FieldOption::NotNull)) {
                sql += " NOT NULL";
            }
            if (hasOption(fields[i].options, FieldOption::Unique)) {
                sql += " UNIQUE";
            }
            if (hasOption(fields[i].options, FieldOption::DefaultNow)) {
                sql += " DEFAULT CURRENT_TIMESTAMP";
            }
        }
        sql += ");";
        
        std::cout << "[Xpress++ ORM] Syncing database table '" << tableName << "'..." << std::endl;
        co_await xp::query(sql);
    }
    
    // CREATE: Insert a new row in database
    static drogon::Task<void> create(const Json::Value& data) {
        std::string tableName = Derived::tableName();
        std::string cols = "";
        std::string vals = "";
        
        int idx = 0;
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (idx > 0) {
                cols += ", ";
                vals += ", ";
            }
            cols += it.name();
            vals += escapeValue(*it);
            idx++;
        }
        
        std::string sql = "INSERT INTO " + tableName + " (" + cols + ") VALUES (" + vals + ");";
        co_await xp::query(sql);
    }
    
    // FIND UNIQUE / FIND FIRST: Get record matching options
    static drogon::Task<Json::Value> findUnique(const Json::Value& where) {
        std::string tableName = Derived::tableName();
        std::string conditions = "";
        
        int idx = 0;
        for (auto it = where.begin(); it != where.end(); ++it) {
            if (idx > 0) {
                conditions += " AND ";
            }
            conditions += it.name() + " = " + escapeValue(*it);
            idx++;
        }
        
        std::string sql = "SELECT * FROM " + tableName;
        if (!conditions.empty()) {
            sql += " WHERE " + conditions;
        }
        sql += " LIMIT 1;";
        
        co_return co_await xp::queryOneJson(sql);
    }
    
    // FIND ALL: Get all records in the table
    static drogon::Task<Json::Value> findAll() {
        co_return co_await findMany();
    }

    // FIND MANY: Get all records matching options
    static drogon::Task<Json::Value> findMany(const Json::Value& where = Json::Value()) {
        std::string tableName = Derived::tableName();
        std::string conditions = "";
        
        if (!where.isNull() && where.isObject()) {
            int idx = 0;
            for (auto it = where.begin(); it != where.end(); ++it) {
                if (idx > 0) {
                    conditions += " AND ";
                }
                conditions += it.name() + " = " + escapeValue(*it);
                idx++;
            }
        }
        
        std::string sql = "SELECT * FROM " + tableName;
        if (!conditions.empty()) {
            sql += " WHERE " + conditions;
        }
        sql += ";";
        
        co_return co_await xp::queryJson(sql);
    }
    
    // UPDATE: Update records matching where with data
    static drogon::Task<void> update(const Json::Value& where, const Json::Value& data) {
        std::string tableName = Derived::tableName();
        std::string sets = "";
        std::string conditions = "";
        
        int idx = 0;
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (idx > 0) {
                sets += ", ";
            }
            sets += it.name() + " = " + escapeValue(*it);
            idx++;
        }
        
        idx = 0;
        for (auto it = where.begin(); it != where.end(); ++it) {
            if (idx > 0) {
                conditions += " AND ";
            }
            conditions += it.name() + " = " + escapeValue(*it);
            idx++;
        }
        
        std::string sql = "UPDATE " + tableName + " SET " + sets;
        if (!conditions.empty()) {
            sql += " WHERE " + conditions;
        }
        sql += ";";
        co_await xp::query(sql);
    }
    
    // DELETE MANY: Remove records matching where
    static drogon::Task<void> deleteMany(const Json::Value& where) {
        std::string tableName = Derived::tableName();
        std::string conditions = "";
        
        int idx = 0;
        for (auto it = where.begin(); it != where.end(); ++it) {
            if (idx > 0) {
                conditions += " AND ";
            }
            conditions += it.name() + " = " + escapeValue(*it);
            idx++;
        }
        
        std::string sql = "DELETE FROM " + tableName;
        if (!conditions.empty()) {
            sql += " WHERE " + conditions;
        }
        sql += ";";
        co_await xp::query(sql);
    }
};

} // namespace xp
