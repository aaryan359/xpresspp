#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <iostream>
#include <variant>
#include "database.h"

#if __has_include(<mongocxx/client.hpp>)
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/uri.hpp>
#endif

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

// SQL Model base class using CRTP (Curiously Recurring Template Pattern)
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
        co_await xp::executeParameterized(sql, {});
    }
    
    // CREATE: Insert a new row in database
    static drogon::Task<void> create(const Json::Value& data) {
        std::string tableName = Derived::tableName();
        auto client = xp::db();
        std::string driver = xp::currentDriver();
        
        std::string cols = "";
        std::string placeholders = "";
        std::vector<QueryParam> params;
        
        int idx = 0;
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (idx > 0) {
                cols += ", ";
                placeholders += ", ";
            }
            cols += it.name();
            placeholders += getPlaceholder(driver, idx + 1);
            params.push_back(jsonToQueryParam(*it));
            idx++;
        }
        
        std::string sql = "INSERT INTO " + tableName + " (" + cols + ") VALUES (" + placeholders + ");";
        co_await xp::executeParameterized(client, sql, params);
    }
    
    // FIND UNIQUE / FIND FIRST: Get record matching options
    static drogon::Task<xp::var> findUnique(const Json::Value& where) {
        std::string tableName = Derived::tableName();
        auto client = xp::db();
        std::string driver = xp::currentDriver();
        
        std::string conditions = "";
        std::vector<QueryParam> params;
        
        int idx = 0;
        for (auto it = where.begin(); it != where.end(); ++it) {
            if (idx > 0) {
                conditions += " AND ";
            }
            conditions += it.name() + " = " + getPlaceholder(driver, idx + 1);
            params.push_back(jsonToQueryParam(*it));
            idx++;
        }
        
        std::string sql = "SELECT * FROM " + tableName;
        if (!conditions.empty()) {
            sql += " WHERE " + conditions;
        }
        sql += " LIMIT 1;";
        
        co_return co_await xp::queryOneJson(sql, params);
    }
    
    // FIND ALL: Get all records in the table
    static drogon::Task<xp::var> findAll() {
        co_return co_await findMany();
    }

    // FIND MANY: Get all records matching options
    static drogon::Task<xp::var> findMany(const Json::Value& where = Json::Value()) {
        std::string tableName = Derived::tableName();
        auto client = xp::db();
        std::string driver = xp::currentDriver();
        
        std::string conditions = "";
        std::vector<QueryParam> params;
        
        if (!where.isNull() && where.isObject()) {
            int idx = 0;
            for (auto it = where.begin(); it != where.end(); ++it) {
                if (idx > 0) {
                    conditions += " AND ";
                }
                conditions += it.name() + " = " + getPlaceholder(driver, idx + 1);
                params.push_back(jsonToQueryParam(*it));
                idx++;
            }
        }
        
        std::string sql = "SELECT * FROM " + tableName;
        if (!conditions.empty()) {
            sql += " WHERE " + conditions;
        }
        sql += ";";
        
        co_return co_await xp::queryJson(sql, params);
    }
    
    // UPDATE: Update records matching where with data
    static drogon::Task<void> update(const Json::Value& where, const Json::Value& data) {
        std::string tableName = Derived::tableName();
        auto client = xp::db();
        std::string driver = xp::currentDriver();
        
        std::string sets = "";
        std::string conditions = "";
        std::vector<QueryParam> params;
        
        int idx = 0;
        for (auto it = data.begin(); it != data.end(); ++it) {
            if (idx > 0) {
                sets += ", ";
            }
            sets += it.name() + " = " + getPlaceholder(driver, idx + 1);
            params.push_back(jsonToQueryParam(*it));
            idx++;
        }
        
        int paramOffset = idx;
        idx = 0;
        for (auto it = where.begin(); it != where.end(); ++it) {
            if (idx > 0) {
                conditions += " AND ";
            }
            conditions += it.name() + " = " + getPlaceholder(driver, paramOffset + idx + 1);
            params.push_back(jsonToQueryParam(*it));
            idx++;
        }
        
        std::string sql = "UPDATE " + tableName + " SET " + sets;
        if (!conditions.empty()) {
            sql += " WHERE " + conditions;
        }
        sql += ";";
        co_await xp::executeParameterized(client, sql, params);
    }
    
    // DELETE MANY: Remove records matching where
    static drogon::Task<void> deleteMany(const Json::Value& where) {
        std::string tableName = Derived::tableName();
        auto client = xp::db();
        std::string driver = xp::currentDriver();
        
        std::string conditions = "";
        std::vector<QueryParam> params;
        
        int idx = 0;
        for (auto it = where.begin(); it != where.end(); ++it) {
            if (idx > 0) {
                conditions += " AND ";
            }
            conditions += it.name() + " = " + getPlaceholder(driver, idx + 1);
            params.push_back(jsonToQueryParam(*it));
            idx++;
        }
        
        std::string sql = "DELETE FROM " + tableName;
        if (!conditions.empty()) {
            sql += " WHERE " + conditions;
        }
        sql += ";";
        co_await xp::executeParameterized(client, sql, params);
    }
};

// MongoDB Model base class using CRTP
#if __has_include(<mongocxx/client.hpp>)
inline bsoncxx::document::value jsonToBson(const Json::Value& json) {
    Json::FastWriter writer;
    std::string json_str = writer.write(json);
    return bsoncxx::from_json(json_str);
}

inline Json::Value bsonToJson(const bsoncxx::document::view& bson) {
    std::string json_str = bsoncxx::to_json(bson);
    Json::CharReaderBuilder builder;
    Json::Value val;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(builder.newCharReader());
    if (reader->parse(json_str.data(), json_str.data() + json_str.size(), &val, &errs)) {
        return val;
    }
    return Json::Value();
}

template <typename Derived>
class MongoModel {
public:
    static std::string collectionName() {
        return Derived::tableName();
    }

    static mongocxx::collection getCollection() {
        return MongoClientManager::get().db()[collectionName()];
    }

    static drogon::Task<void> sync() {
        // No-op for MongoDB collection creation
        co_return;
    }

    static drogon::Task<void> create(const Json::Value& data) {
        auto coll = getCollection();
        auto doc = jsonToBson(data);
        coll.insert_one(doc.view());
        co_return;
    }

    static drogon::Task<xp::var> findUnique(const Json::Value& where) {
        auto coll = getCollection();
        auto filter = jsonToBson(where);
        auto result = coll.find_one(filter.view());
        if (result) {
            co_return bsonToJson(result->view());
        }
        co_return Json::Value();
    }

    static drogon::Task<xp::var> findAll() {
        co_return co_await findMany();
    }

    static drogon::Task<xp::var> findMany(const Json::Value& where = Json::Value()) {
        auto coll = getCollection();
        auto filter = (where.isNull() || !where.isObject()) ? 
            bsoncxx::document::value(bsoncxx::builder::basic::make_document()) : 
            jsonToBson(where);
        
        auto cursor = coll.find(filter.view());
        Json::Value arr(Json::arrayValue);
        for (auto&& doc : cursor) {
            arr.append(bsonToJson(doc));
        }
        co_return arr;
    }

    static drogon::Task<void> update(const Json::Value& where, const Json::Value& data) {
        auto coll = getCollection();
        auto filter = jsonToBson(where);
        
        Json::Value setOp;
        setOp["$set"] = data;
        auto updateDoc = jsonToBson(setOp);
        
        coll.update_many(filter.view(), updateDoc.view());
        co_return;
    }

    static drogon::Task<void> deleteMany(const Json::Value& where) {
        auto coll = getCollection();
        auto filter = jsonToBson(where);
        coll.delete_many(filter.view());
        co_return;
    }
};
#else
template <typename Derived>
class MongoModel {
public:
    static drogon::Task<void> sync() {
        throw std::runtime_error("MongoDB driver is not installed on this system.");
        co_return;
    }
    static drogon::Task<void> create(const Json::Value&) {
        throw std::runtime_error("MongoDB driver is not installed on this system.");
        co_return;
    }
    static drogon::Task<xp::var> findUnique(const Json::Value&) {
        throw std::runtime_error("MongoDB driver is not installed on this system.");
        co_return xp::var();
    }
    static drogon::Task<xp::var> findAll() {
        throw std::runtime_error("MongoDB driver is not installed on this system.");
        co_return xp::var();
    }
    static drogon::Task<xp::var> findMany(const Json::Value&) {
        throw std::runtime_error("MongoDB driver is not installed on this system.");
        co_return xp::var();
    }
    static drogon::Task<void> update(const Json::Value&, const Json::Value&) {
        throw std::runtime_error("MongoDB driver is not installed on this system.");
        co_return;
    }
    static drogon::Task<void> deleteMany(const Json::Value&) {
        throw std::runtime_error("MongoDB driver is not installed on this system.");
        co_return;
    }
};
#endif

class Table {
private:
    std::string name_;

public:
    explicit Table(std::string name) : name_(std::move(name)) {}

    drogon::Task<void> create(const Json::Value& data) {
        if (xp::currentDriver() == "mongodb") {
#if __has_include(<mongocxx/client.hpp>)
            auto coll = MongoClientManager::get().db()[name_];
            auto doc = jsonToBson(data);
            coll.insert_one(doc.view());
#else
            throw std::runtime_error("MongoDB driver is not installed on this system.");
#endif
            co_return;
        } else {
            std::string driver = xp::currentDriver();
            auto client = xp::db();
            std::string cols = "";
            std::string placeholders = "";
            std::vector<QueryParam> params;
            
            int idx = 0;
            for (auto it = data.begin(); it != data.end(); ++it) {
                if (idx > 0) {
                    cols += ", ";
                    placeholders += ", ";
                }
                cols += it.name();
                placeholders += getPlaceholder(driver, idx + 1);
                params.push_back(jsonToQueryParam(*it));
                idx++;
            }
            
            std::string sql = "INSERT INTO " + name_ + " (" + cols + ") VALUES (" + placeholders + ");";
            co_await xp::executeParameterized(client, sql, params);
        }
    }

    drogon::Task<xp::var> findUnique(const Json::Value& where) {
        if (xp::currentDriver() == "mongodb") {
#if __has_include(<mongocxx/client.hpp>)
            auto coll = MongoClientManager::get().db()[name_];
            auto filter = jsonToBson(where);
            auto result = coll.find_one(filter.view());
            if (result) {
                co_return bsonToJson(result->view());
            }
#else
            throw std::runtime_error("MongoDB driver is not installed on this system.");
#endif
            co_return Json::Value();
        } else {
            std::string driver = xp::currentDriver();
            auto client = xp::db();
            std::string conditions = "";
            std::vector<QueryParam> params;
            
            int idx = 0;
            for (auto it = where.begin(); it != where.end(); ++it) {
                if (idx > 0) {
                    conditions += " AND ";
                }
                conditions += it.name() + " = " + getPlaceholder(driver, idx + 1);
                params.push_back(jsonToQueryParam(*it));
                idx++;
            }
            
            std::string sql = "SELECT * FROM " + name_;
            if (!conditions.empty()) {
                sql += " WHERE " + conditions;
            }
            sql += " LIMIT 1;";
            
            co_return co_await xp::queryOneJson(sql, params);
        }
    }

    drogon::Task<xp::var> findMany(const Json::Value& where = Json::Value()) {
        if (xp::currentDriver() == "mongodb") {
#if __has_include(<mongocxx/client.hpp>)
            auto coll = MongoClientManager::get().db()[name_];
            auto filter = (where.isNull() || !where.isObject()) ? 
                bsoncxx::document::value(bsoncxx::builder::basic::make_document()) : 
                jsonToBson(where);
            auto cursor = coll.find(filter.view());
            Json::Value arr(Json::arrayValue);
            for (auto&& doc : cursor) {
                arr.append(bsonToJson(doc));
            }
            co_return arr;
#else
            throw std::runtime_error("MongoDB driver is not installed on this system.");
#endif
            co_return Json::Value();
        } else {
            std::string driver = xp::currentDriver();
            auto client = xp::db();
            std::string conditions = "";
            std::vector<QueryParam> params;
            
            if (!where.isNull() && where.isObject()) {
                int idx = 0;
                for (auto it = where.begin(); it != where.end(); ++it) {
                    if (idx > 0) {
                        conditions += " AND ";
                    }
                    conditions += it.name() + " = " + getPlaceholder(driver, idx + 1);
                    params.push_back(jsonToQueryParam(*it));
                    idx++;
                }
            }
            
            std::string sql = "SELECT * FROM " + name_;
            if (!conditions.empty()) {
                sql += " WHERE " + conditions;
            }
            sql += ";";
            
            co_return co_await xp::queryJson(sql, params);
        }
    }

    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) {
        if (xp::currentDriver() == "mongodb") {
#if __has_include(<mongocxx/client.hpp>)
            auto coll = MongoClientManager::get().db()[name_];
            auto filter = jsonToBson(where);
            Json::Value setOp;
            setOp["$set"] = data;
            auto updateDoc = jsonToBson(setOp);
            coll.update_many(filter.view(), updateDoc.view());
#else
            throw std::runtime_error("MongoDB driver is not installed on this system.");
#endif
            co_return;
        } else {
            std::string driver = xp::currentDriver();
            auto client = xp::db();
            std::string sets = "";
            std::string conditions = "";
            std::vector<QueryParam> params;
            
            int idx = 0;
            for (auto it = data.begin(); it != data.end(); ++it) {
                if (idx > 0) {
                    sets += ", ";
                }
                sets += it.name() + " = " + getPlaceholder(driver, idx + 1);
                params.push_back(jsonToQueryParam(*it));
                idx++;
            }
            
            int paramOffset = idx;
            idx = 0;
            for (auto it = where.begin(); it != where.end(); ++it) {
                if (idx > 0) {
                    conditions += " AND ";
                }
                conditions += it.name() + " = " + getPlaceholder(driver, paramOffset + idx + 1);
                params.push_back(jsonToQueryParam(*it));
                idx++;
            }
            
            std::string sql = "UPDATE " + name_ + " SET " + sets;
            if (!conditions.empty()) {
                sql += " WHERE " + conditions;
            }
            sql += ";";
            co_await xp::executeParameterized(client, sql, params);
        }
    }

    drogon::Task<void> deleteMany(const Json::Value& where) {
        if (xp::currentDriver() == "mongodb") {
#if __has_include(<mongocxx/client.hpp>)
            auto coll = MongoClientManager::get().db()[name_];
            auto filter = jsonToBson(where);
            coll.delete_many(filter.view());
#else
            throw std::runtime_error("MongoDB driver is not installed on this system.");
#endif
            co_return;
        } else {
            std::string driver = xp::currentDriver();
            auto client = xp::db();
            std::string conditions = "";
            std::vector<QueryParam> params;
            
            int idx = 0;
            for (auto it = where.begin(); it != where.end(); ++it) {
                if (idx > 0) {
                    conditions += " AND ";
                }
                conditions += it.name() + " = " + getPlaceholder(driver, idx + 1);
                params.push_back(jsonToQueryParam(*it));
                idx++;
            }
            
            std::string sql = "DELETE FROM " + name_;
            if (!conditions.empty()) {
                sql += " WHERE " + conditions;
            }
            sql += ";";
            co_await xp::executeParameterized(client, sql, params);
        }
    }
};

inline Table table(std::string name) {
    return Table(std::move(name));
}

} // namespace xp
