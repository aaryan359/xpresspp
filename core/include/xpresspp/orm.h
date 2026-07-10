#pragma once
#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <iostream>
#include <variant>
#include "database.h"

namespace xp {

// SQL Model base class using CRTP (Curiously Recurring Template Pattern)
template <typename Derived>
class Model {
public:
    // Synchronize schema (create table IF NOT EXISTS or ALTER TABLE)
    static drogon::Task<void> sync() {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        std::cout << "[Xpress++ ORM] Syncing database table '" << tableName << "'..." << std::endl;
        co_await xp::DatabaseManager::instance().driver()->syncSchema(tableName, schema);
    }
    
    // CREATE: Insert a new row in database
    static drogon::Task<void> create(const Json::Value& data) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        co_await xp::DatabaseManager::instance().driver()->execute(tableName, schema, OpType::Create, data);
    }

    static drogon::Task<void> create(drogon::orm::TransactionPtr tx, const Json::Value& data) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        co_await xp::DatabaseManager::instance().driver()->executeTx(tx, tableName, schema, OpType::Create, data);
    }
    
    // FIND UNIQUE / FIND FIRST: Get record matching options
    static drogon::Task<xp::var> findUnique(const Json::Value& query) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        co_return co_await xp::DatabaseManager::instance().driver()->execute(tableName, schema, OpType::FindUnique, query);
    }

    static drogon::Task<xp::var> findUnique(drogon::orm::TransactionPtr tx, const Json::Value& query) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        co_return co_await xp::DatabaseManager::instance().driver()->executeTx(tx, tableName, schema, OpType::FindUnique, query);
    }
    
    // FIND ALL: Get all records in the table
    static drogon::Task<xp::var> findAll() {
        co_return co_await findMany();
    }

    static drogon::Task<xp::var> findAll(drogon::orm::TransactionPtr tx) {
        co_return co_await findMany(tx);
    }

    // FIND MANY: Get all records matching options
    static drogon::Task<xp::var> findMany(const Json::Value& query = Json::Value()) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        co_return co_await xp::DatabaseManager::instance().driver()->execute(tableName, schema, OpType::FindMany, query);
    }

    static drogon::Task<xp::var> findMany(drogon::orm::TransactionPtr tx, const Json::Value& query = Json::Value()) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        co_return co_await xp::DatabaseManager::instance().driver()->executeTx(tx, tableName, schema, OpType::FindMany, query);
    }
    
    // UPDATE: Update records matching where with data
    static drogon::Task<void> update(const Json::Value& where, const Json::Value& data) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        xp::var query;
        query["where"] = where;
        query["data"] = data;
        co_await xp::DatabaseManager::instance().driver()->execute(tableName, schema, OpType::Update, query);
    }

    static drogon::Task<void> update(drogon::orm::TransactionPtr tx, const Json::Value& where, const Json::Value& data) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        xp::var query;
        query["where"] = where;
        query["data"] = data;
        co_await xp::DatabaseManager::instance().driver()->executeTx(tx, tableName, schema, OpType::Update, query);
    }
    
    // DELETE MANY: Remove records matching where
    static drogon::Task<void> deleteMany(const Json::Value& where) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        xp::var query;
        query["where"] = where;
        co_await xp::DatabaseManager::instance().driver()->execute(tableName, schema, OpType::Delete, query);
    }

    static drogon::Task<void> deleteMany(drogon::orm::TransactionPtr tx, const Json::Value& where) {
        std::string tableName = Derived::tableName();
        auto schema = Derived::schema();
        xp::var query;
        query["where"] = where;
        co_await xp::DatabaseManager::instance().driver()->executeTx(tx, tableName, schema, OpType::Delete, query);
    }
};

// MongoModel base class using CRTP. It can inherit from Model to use the same IDatabaseDriver execution path
template <typename Derived>
class MongoModel : public Model<Derived> {};

// Table client helper for raw database operations without predefined models
class Table {
private:
    std::string name_;

public:
    explicit Table(std::string name) : name_(std::move(name)) {}

    drogon::Task<void> create(const Json::Value& data) {
        co_await xp::DatabaseManager::instance().driver()->execute(name_, {}, OpType::Create, data);
    }

    drogon::Task<xp::var> findUnique(const Json::Value& where) {
        xp::var query;
        query["where"] = where;
        co_return co_await xp::DatabaseManager::instance().driver()->execute(name_, {}, OpType::FindUnique, query);
    }

    drogon::Task<xp::var> findMany(const Json::Value& where = Json::Value()) {
        xp::var query;
        query["where"] = where;
        co_return co_await xp::DatabaseManager::instance().driver()->execute(name_, {}, OpType::FindMany, query);
    }

    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) {
        xp::var query;
        query["where"] = where;
        query["data"] = data;
        co_await xp::DatabaseManager::instance().driver()->execute(name_, {}, OpType::Update, query);
    }

    drogon::Task<void> deleteMany(const Json::Value& where) {
        xp::var query;
        query["where"] = where;
        co_await xp::DatabaseManager::instance().driver()->execute(name_, {}, OpType::Delete, query);
    }
};

inline Table table(std::string name) {
    return Table(std::move(name));
}

} // namespace xp
