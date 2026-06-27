#pragma once

#include <drogon/drogon.h>
#include <json/json.h>
#include <string>
#include <vector>
#include <variant>
#include <type_traits>
#include <memory>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include "utils.h"

namespace xp {

enum class OpType {
    Create,
    FindUnique,
    FindMany,
    Update,
    Delete
};

enum class FieldType {
    Serial,
    Integer,
    Text,
    Boolean,
    Double,
    Timestamp
};

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

inline Json::Value parseFieldToValue(const std::string& str) {
    if (str.empty()) {
        return Json::Value("");
    }
    
    if (str == "t" || str == "true" || str == "TRUE") {
        return Json::Value(true);
    }
    if (str == "f" || str == "false" || str == "FALSE") {
        return Json::Value(false);
    }
    
    if ((str.front() == '{' && str.back() == '}') || (str.front() == '[' && str.back() == ']')) {
        Json::Value nested;
        Json::Reader reader;
        if (reader.parse(str, nested)) {
            return nested;
        }
    }
    
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

class IDatabaseDriver {
public:
    virtual ~IDatabaseDriver() = default;
    virtual std::string name() const = 0;
    virtual void connect(const std::string& connection_url) = 0;
    virtual void connect(const DbConfig& config) = 0;
    virtual void disconnect() = 0;
    virtual drogon::Task<void> syncSchema(const std::string& model_name, const Schema& schema) = 0;
    virtual drogon::Task<xp::var> execute(
        const std::string& model_name,
        const Schema& schema,
        OpType op,
        const xp::var& query
    ) = 0;
};

struct SqlQuery {
    std::string sql;
    std::vector<QueryParam> params;
};

class BaseSqlDriver : public IDatabaseDriver {
protected:
    std::string driver_name_;
    std::string db_name_ = "default";

    std::string fieldTypeToSql(FieldType type) {
        switch (type) {
            case FieldType::Serial:    return driver_name_ == "postgresql" ? "SERIAL" : "INTEGER PRIMARY KEY AUTOINCREMENT";
            case FieldType::Integer:   return "BIGINT";
            case FieldType::Text:      return "VARCHAR(255)";
            case FieldType::Boolean:   return "BOOLEAN";
            case FieldType::Double:    return "DOUBLE PRECISION";
            case FieldType::Timestamp: return "TIMESTAMP";
        }
        return "TEXT";
    }

    SqlQuery compileSqlQuery(const std::string& table_name, OpType op, const xp::var& query) {
        SqlQuery q;
        std::string where_clause = "";
        std::string order_clause = "";
        std::string limit_clause = "";
        std::string offset_clause = "";

        if (query.isMember("where") && query["where"].isObject()) {
            const auto& where = query["where"];
            int param_idx = 1;
            for (auto it = where.begin(); it != where.end(); ++it) {
                std::string field = it.name();
                const auto& val = *it;

                if (it != where.begin()) {
                    where_clause += " AND ";
                } else {
                    where_clause += " WHERE ";
                }

                if (val.isObject()) {
                    bool first_op = true;
                    for (auto op_it = val.begin(); op_it != val.end(); ++op_it) {
                        std::string op_name = op_it.name();
                        const auto& op_val = *op_it;

                        if (!first_op) {
                            where_clause += " AND ";
                        }
                        first_op = false;

                        std::string sql_op = "=";
                        if (op_name == "gt") sql_op = ">";
                        else if (op_name == "gte") sql_op = ">=";
                        else if (op_name == "lt") sql_op = "<";
                        else if (op_name == "lte") sql_op = "<=";
                        else if (op_name == "not") sql_op = "!=";
                        else if (op_name == "contains") {
                            sql_op = "LIKE";
                            q.params.push_back("%" + op_val.asString() + "%");
                            where_clause += field + " " + sql_op + " " + getPlaceholder(driver_name_, param_idx++);
                            continue;
                        }
                        else if (op_name == "startsWith") {
                            sql_op = "LIKE";
                            q.params.push_back(op_val.asString() + "%");
                            where_clause += field + " " + sql_op + " " + getPlaceholder(driver_name_, param_idx++);
                            continue;
                        }
                        else if (op_name == "endsWith") {
                            sql_op = "LIKE";
                            q.params.push_back("%" + op_val.asString());
                            where_clause += field + " " + sql_op + " " + getPlaceholder(driver_name_, param_idx++);
                            continue;
                        }

                        where_clause += field + " " + sql_op + " " + getPlaceholder(driver_name_, param_idx++);
                        q.params.push_back(jsonToQueryParam(op_val));
                    }
                } else {
                    where_clause += field + " = " + getPlaceholder(driver_name_, param_idx++);
                    q.params.push_back(jsonToQueryParam(val));
                }
            }
        }

        if (query.isMember("orderBy") && query["orderBy"].isObject()) {
            const auto& order = query["orderBy"];
            for (auto it = order.begin(); it != order.end(); ++it) {
                if (it != order.begin()) order_clause += ", ";
                else order_clause += " ORDER BY ";
                order_clause += it.name() + " " + (it->asString() == "desc" ? "DESC" : "ASC");
            }
        }

        if (query.isMember("take") && query["take"].isInt()) {
            limit_clause = " LIMIT " + std::to_string(query["take"].asInt());
        }

        if (query.isMember("skip") && query["skip"].isInt()) {
            offset_clause = " OFFSET " + std::to_string(query["skip"].asInt());
        }

        if (op == OpType::FindMany || op == OpType::FindUnique) {
            q.sql = "SELECT * FROM " + table_name + where_clause + order_clause + limit_clause + offset_clause;
            if (op == OpType::FindUnique) {
                q.sql += " LIMIT 1";
            }
        } else if (op == OpType::Delete) {
            q.sql = "DELETE FROM " + table_name + where_clause;
        } else if (op == OpType::Create) {
            std::string cols = "";
            std::string placeholders = "";
            const auto& data = query.isMember("data") ? query["data"] : query;
            int idx = 0;
            for (auto it = data.begin(); it != data.end(); ++it) {
                if (idx > 0) {
                    cols += ", ";
                    placeholders += ", ";
                }
                cols += it.name();
                placeholders += getPlaceholder(driver_name_, idx + 1);
                q.params.push_back(jsonToQueryParam(*it));
                idx++;
            }
            q.sql = "INSERT INTO " + table_name + " (" + cols + ") VALUES (" + placeholders + ");";
        } else if (op == OpType::Update) {
            std::string sets = "";
            const auto& data = query["data"];
            int idx = 0;
            for (auto it = data.begin(); it != data.end(); ++it) {
                if (idx > 0) {
                    sets += ", ";
                }
                sets += it.name() + " = " + getPlaceholder(driver_name_, idx + 1);
                q.params.push_back(jsonToQueryParam(*it));
                idx++;
            }

            int param_offset = idx;
            if (query.isMember("where") && query["where"].isObject()) {
                const auto& where = query["where"];
                for (auto it = where.begin(); it != where.end(); ++it) {
                    if (it != where.begin()) {
                        where_clause += " AND ";
                    } else {
                        where_clause += " WHERE ";
                    }
                    where_clause += it.name() + " = " + getPlaceholder(driver_name_, ++param_offset);
                    q.params.push_back(jsonToQueryParam(*it));
                }
            }
            q.sql = "UPDATE " + table_name + " SET " + sets + where_clause + ";";
        }

        return q;
    }

public:
    explicit BaseSqlDriver(std::string driver_name) : driver_name_(std::move(driver_name)) {}

    std::string name() const override { return driver_name_; }

    void connect(const std::string&) override {}
    void connect(const DbConfig& config) override {
        db_name_ = config.name;
    }
    void disconnect() override {}

    drogon::Task<void> syncSchema(const std::string& model_name, const Schema& schema) override {
        auto client = drogon::app().getDbClient(db_name_);
        if (!client) {
            co_return;
        }

        std::string check_sql = "";
        if (driver_name_ == "postgresql") {
            check_sql = "SELECT column_name FROM information_schema.columns WHERE table_name = '" + model_name + "';";
        } else if (driver_name_ == "sqlite3") {
            check_sql = "PRAGMA table_info(" + model_name + ");";
        }

        bool table_exists = false;
        std::vector<std::string> existing_cols;

        try {
            auto result = co_await executeParameterized(client, check_sql, {});
            if (!result.empty()) {
                table_exists = true;
                for (const auto& row : result) {
                    if (driver_name_ == "postgresql") {
                        existing_cols.push_back(row["column_name"].as<std::string>());
                    } else if (driver_name_ == "sqlite3") {
                        existing_cols.push_back(row["name"].as<std::string>());
                    }
                }
            }
        } catch (...) {
        }

        if (!table_exists) {
            std::string sql = "CREATE TABLE " + model_name + " (";
            for (size_t i = 0; i < schema.size(); ++i) {
                if (i > 0) sql += ", ";
                sql += schema[i].name + " " + fieldTypeToSql(schema[i].type);
                
                if (hasOption(schema[i].options, FieldOption::PrimaryKey)) {
                    if (driver_name_ == "sqlite3" && schema[i].type == FieldType::Serial) {
                    } else {
                        sql += " PRIMARY KEY";
                    }
                }
                if (hasOption(schema[i].options, FieldOption::NotNull)) {
                    sql += " NOT NULL";
                }
                if (hasOption(schema[i].options, FieldOption::Unique) && !(driver_name_ == "sqlite3" && schema[i].type == FieldType::Serial)) {
                    sql += " UNIQUE";
                }
                if (hasOption(schema[i].options, FieldOption::DefaultNow)) {
                    sql += " DEFAULT CURRENT_TIMESTAMP";
                }
            }
            sql += ");";
            co_await executeParameterized(client, sql, {});
        } else {
            for (const auto& field : schema) {
                bool found = false;
                for (const auto& col : existing_cols) {
                    if (col == field.name) {
                        found = true;
                        break;
                    }
                }

                if (!found) {
                    std::string sql = "ALTER TABLE " + model_name + " ADD COLUMN " + field.name + " " + fieldTypeToSql(field.type);
                    if (hasOption(field.options, FieldOption::NotNull)) {
                        if (driver_name_ == "sqlite3") {
                            sql += " DEFAULT ''";
                        }
                    }
                    sql += ";";
                    try {
                        co_await executeParameterized(client, sql, {});
                        std::cout << "[Xpress++ ORM] Safely altered table '" << model_name 
                                  << "' adding column '" << field.name << "'" << std::endl;
                    } catch (const std::exception& e) {
                        std::cerr << "[Xpress++ ORM Error] Failed to alter table adding column: " << e.what() << std::endl;
                    }
                }
            }
        }
    }

    drogon::Task<xp::var> execute(
        const std::string& model_name,
        const Schema& schema,
        OpType op,
        const xp::var& query
    ) override {
        auto client = drogon::app().getDbClient(db_name_);
        if (!client) {
            throw std::runtime_error("Database is not initialized.");
        }

        SqlQuery q = compileSqlQuery(model_name, op, query);
        auto result = co_await executeParameterized(client, q.sql, q.params);

        if (op == OpType::FindUnique) {
            if (result.empty()) {
                co_return xp::var();
            }
            co_return rowToJson(result, result[0]);
        } else if (op == OpType::FindMany) {
            co_return resultToJson(result);
        }

        co_return xp::var();
    }
};

class PostgreSqlDriver : public BaseSqlDriver {
public:
    PostgreSqlDriver() : BaseSqlDriver("postgresql") {}
};

class SqliteDriver : public BaseSqlDriver {
public:
    SqliteDriver() : BaseSqlDriver("sqlite3") {}
};

#if __has_include(<mongocxx/client.hpp>)
#include <bsoncxx/json.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <mongocxx/client.hpp>
#include <mongocxx/database.hpp>
#include <mongocxx/uri.hpp>

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

class MongoDbDriver : public IDatabaseDriver {
private:
    std::string uri_str_;
    std::string db_name_;

public:
    std::string name() const override { return "mongodb"; }

    void connect(const std::string& connection_url) override {
        uri_str_ = connection_url;
        db_name_ = "default";
        
        auto last_slash = connection_url.rfind('/');
        if (last_slash != std::string::npos && last_slash > 9) {
            auto db_part = connection_url.substr(last_slash + 1);
            auto question_mark = db_part.find('?');
            if (question_mark != std::string::npos) {
                db_name_ = db_part.substr(0, question_mark);
            } else {
                db_name_ = db_part;
            }
        }
        MongoClientManager::get().connect(uri_str_, db_name_);
    }

    void connect(const DbConfig& config) override {
        std::string url = "mongodb://";
        if (!config.username.empty()) {
            url += config.username;
            if (!config.password.empty()) {
                url += ":" + config.password;
            }
            url += "@";
        }
        url += config.host;
        if (config.port > 0) {
            url += ":" + std::to_string(config.port);
        }
        url += "/" + config.database;
        connect(url);
    }

    void disconnect() override {}

    drogon::Task<void> syncSchema(const std::string&, const Schema&) override {
        co_return;
    }

    drogon::Task<xp::var> execute(
        const std::string& model_name,
        const Schema&,
        OpType op,
        const xp::var& query
    ) override {
        auto coll = MongoClientManager::get().db()[model_name];

        if (op == OpType::Create) {
            const auto& data = query.isMember("data") ? query["data"] : query;
            auto doc = jsonToBson(data);
            coll.insert_one(doc.view());
            co_return xp::var();
        } else if (op == OpType::FindUnique || op == OpType::FindMany) {
            xp::var filter = (query.isMember("where") && query["where"].isObject()) ? query["where"] : Json::Value(Json::objectValue);
            auto bson_filter = jsonToBson(filter);

            if (op == OpType::FindUnique) {
                auto result = coll.find_one(bson_filter.view());
                if (result) {
                    co_return bsonToJson(result->view());
                }
                co_return xp::var();
            } else {
                auto cursor = coll.find(bson_filter.view());
                Json::Value arr(Json::arrayValue);
                for (auto&& doc : cursor) {
                    arr.append(bsonToJson(doc));
                }
                co_return arr;
            }
        } else if (op == OpType::Update) {
            xp::var filter = (query.isMember("where") && query["where"].isObject()) ? query["where"] : Json::Value(Json::objectValue);
            auto bson_filter = jsonToBson(filter);

            const auto& data = query["data"];
            Json::Value setOp;
            setOp["$set"] = data;
            auto updateDoc = jsonToBson(setOp);

            coll.update_many(bson_filter.view(), updateDoc.view());
            co_return xp::var();
        } else if (op == OpType::Delete) {
            xp::var filter = (query.isMember("where") && query["where"].isObject()) ? query["where"] : Json::Value(Json::objectValue);
            auto bson_filter = jsonToBson(filter);
            coll.delete_many(bson_filter.view());
            co_return xp::var();
        }

        co_return xp::var();
    }
};
#else
class MongoDbDriver : public IDatabaseDriver {
public:
    std::string name() const override { return "mongodb"; }
    void connect(const std::string&) override { throw std::runtime_error("MongoDB driver is not installed."); }
    void connect(const DbConfig&) override { throw std::runtime_error("MongoDB driver is not installed."); }
    void disconnect() override {}
    drogon::Task<void> syncSchema(const std::string&, const Schema&) override { co_return; }
    drogon::Task<xp::var> execute(const std::string&, const Schema&, OpType, const xp::var&) override {
        throw std::runtime_error("MongoDB driver is not installed.");
        co_return xp::var();
    }
};
#endif

class DatabaseManager {
private:
    std::unique_ptr<IDatabaseDriver> active_driver_;
    DatabaseManager() = default;

public:
    static DatabaseManager& instance() {
        static DatabaseManager inst;
        return inst;
    }

    void connect(const std::string& connection_url) {
        if (connection_url.rfind("postgresql://", 0) == 0 || connection_url.rfind("postgres://", 0) == 0) {
            active_driver_ = std::make_unique<PostgreSqlDriver>();
        } else if (connection_url.rfind("mongodb://", 0) == 0 || connection_url.rfind("mongodb+srv://", 0) == 0) {
            active_driver_ = std::make_unique<MongoDbDriver>();
        } else if (connection_url.rfind("sqlite://", 0) == 0 || connection_url.rfind("sqlite3://", 0) == 0) {
            active_driver_ = std::make_unique<SqliteDriver>();
        } else {
            throw std::runtime_error("Unsupported database driver scheme in URL: " + connection_url);
        }
        active_driver_->connect(connection_url);
    }

    void connect(const DbConfig& config) {
        if (config.driver == "postgresql" || config.driver == "postgres") {
            active_driver_ = std::make_unique<PostgreSqlDriver>();
        } else if (config.driver == "mongodb") {
            active_driver_ = std::make_unique<MongoDbDriver>();
        } else if (config.driver == "sqlite3" || config.driver == "sqlite") {
            active_driver_ = std::make_unique<SqliteDriver>();
        } else {
            throw std::runtime_error("Unsupported database driver: " + config.driver);
        }
        active_driver_->connect(config);
    }

    IDatabaseDriver* driver() {
        if (!active_driver_) {
            throw std::runtime_error("Database is not connected. Call app.database() first.");
        }
        return active_driver_.get();
    }

    static std::string trim(std::string str) {
        str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
            return !std::isspace(ch);
        }));
        str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
            return !std::isspace(ch);
        }).base(), str.end());
        return str;
    }

    drogon::Task<void> runMigrations() {
        auto client = drogon::app().getDbClient("default");
        if (!client) co_return;

        std::vector<QueryParam> emptyParams;

        // 1. Create _xp_migrations table if not exists
        std::string create_table_sql = 
            "CREATE TABLE IF NOT EXISTS _xp_migrations ("
            "  id VARCHAR(255) PRIMARY KEY,"
            "  name VARCHAR(255) NOT NULL,"
            "  appliedAt TIMESTAMP DEFAULT CURRENT_TIMESTAMP"
            ");";
        co_await executeParameterized(client, create_table_sql, emptyParams);

        // 2. Read applied migrations
        std::vector<std::string> applied;
        try {
            auto result = co_await executeParameterized(client, "SELECT id FROM _xp_migrations ORDER BY id ASC;", emptyParams);
            for (const auto& row : result) {
                applied.push_back(row["id"].as<std::string>());
            }
        } catch (...) {}

        // 3. Scan migrations/ folder
        std::vector<std::string> migration_dirs;
        if (std::filesystem::exists("migrations")) {
            for (const auto& entry : std::filesystem::directory_iterator("migrations")) {
                if (entry.is_directory()) {
                    migration_dirs.push_back(entry.path().filename().string());
                }
            }
        }
        std::sort(migration_dirs.begin(), migration_dirs.end());

        // 4. Run pending migrations
        for (const auto& dir : migration_dirs) {
            if (std::find(applied.begin(), applied.end(), dir) == applied.end()) {
                std::string up_path = "migrations/" + dir + "/up.sql";
                if (!std::filesystem::exists(up_path)) continue;

                std::ifstream f(up_path);
                if (!f.is_open()) continue;
                std::string sql((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                if (sql.empty()) continue;

                std::cout << "[Xpress++ Migration] Applying migration: " << dir << std::endl;
                bool failed = false;
                std::string error_msg;
                try {
                    // Split SQL by semicolon and execute each statement
                    std::stringstream ss(sql);
                    std::string stmt;
                    while (std::getline(ss, stmt, ';')) {
                        stmt = trim(stmt);
                        if (!stmt.empty()) {
                            co_await executeParameterized(client, stmt, emptyParams);
                        }
                    }

                    // Record applied migration
                    std::string name = dir;
                    auto pos = dir.find('_');
                    if (pos != std::string::npos) {
                        name = dir.substr(pos + 1);
                    }
                    std::vector<QueryParam> recordParams;
                    recordParams.push_back(QueryParam(dir));
                    recordParams.push_back(QueryParam(name));
                    co_await executeParameterized(
                        client, 
                        "INSERT INTO _xp_migrations (id, name) VALUES ($1, $2);", 
                        recordParams
                    );
                    std::cout << "[Xpress++ Migration] Successfully applied migration: " << dir << std::endl;
                } catch (const std::exception& e) {
                    failed = true;
                    error_msg = e.what();
                }

                if (failed) {
                    std::cerr << "[Xpress++ Migration Error] Migration " << dir << " failed: " << error_msg << std::endl;
                    // Try rolling back if down.sql exists
                    std::string down_path = "migrations/" + dir + "/down.sql";
                    if (std::filesystem::exists(down_path)) {
                        std::cout << "[Xpress++ Migration] Rolling back failed migration..." << std::endl;
                        try {
                            std::ifstream df(down_path);
                            std::string down_sql((std::istreambuf_iterator<char>(df)), std::istreambuf_iterator<char>());
                            std::stringstream dss(down_sql);
                            std::string dstmt;
                            while (std::getline(dss, dstmt, ';')) {
                                dstmt = trim(dstmt);
                                if (!dstmt.empty()) {
                                    co_await executeParameterized(client, dstmt, emptyParams);
                                }
                            }
                        } catch (...) {}
                    }
                    throw std::runtime_error(error_msg);
                }
            }
        }
    }

    drogon::Task<void> rollbackLastMigration() {
        auto client = drogon::app().getDbClient("default");
        if (!client) co_return;

        std::vector<QueryParam> emptyParams;

        // 1. Get last applied migration
        std::string last_id = "";
        try {
            auto result = co_await executeParameterized(client, "SELECT id FROM _xp_migrations ORDER BY id DESC LIMIT 1;", emptyParams);
            if (!result.empty()) {
                last_id = result[0]["id"].as<std::string>();
            }
        } catch (...) {}

        if (last_id.empty()) {
            std::cout << "[Xpress++ Migration] No migrations to roll back." << std::endl;
            co_return;
        }

        std::string down_path = "migrations/" + last_id + "/down.sql";
        if (!std::filesystem::exists(down_path)) {
            std::cerr << "[Xpress++ Migration Error] down.sql not found for migration: " << last_id << std::endl;
            co_return;
        }

        std::ifstream f(down_path);
        if (!f.is_open()) co_return;
        std::string sql((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

        std::cout << "[Xpress++ Migration] Rolling back migration: " << last_id << std::endl;
        try {
            std::stringstream ss(sql);
            std::string stmt;
            while (std::getline(ss, stmt, ';')) {
                stmt = trim(stmt);
                if (!stmt.empty()) {
                    co_await executeParameterized(client, stmt, emptyParams);
                }
            }

            std::vector<QueryParam> delParams;
            delParams.push_back(QueryParam(last_id));
            co_await executeParameterized(client, "DELETE FROM _xp_migrations WHERE id = $1;", delParams);
            std::cout << "[Xpress++ Migration] Successfully rolled back migration: " << last_id << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "[Xpress++ Migration Error] Rollback failed: " << e.what() << std::endl;
            throw;
        }
    }
};

inline std::string currentDriver() {
    try {
        return DatabaseManager::instance().driver()->name();
    } catch (...) {
        return "sqlite3";
    }
}

inline drogon::orm::DbClientPtr db(const std::string& name = "default") {
    return drogon::app().getDbClient(name);
}

} // namespace xp
