#pragma once
#include <xpresspp/xpresspp.h>
#include <string>
#include <vector>

class User : public xp::Model<User> {
public:
    using xp::Model<User>::create;

    static std::string tableName() { return "users"; }

    static xp::Schema schema() {
        return {
            {"id", xp::FieldType::Serial, xp::FieldOption::PrimaryKey},
            {"username", xp::FieldType::Text, xp::FieldOption::NotNull | xp::FieldOption::Unique}
        };
    }

    static drogon::Task<Json::Value> findById(int64_t val) {
        Json::Value where;
        where["id"] = val;
        co_return co_await findUnique(where);
    }

    static drogon::Task<Json::Value> findByUsername(const std::string& val) {
        Json::Value where;
        where["username"] = val;
        co_return co_await findUnique(where);
    }

};

class Post : public xp::Model<Post> {
public:
    using xp::Model<Post>::create;

    static std::string tableName() { return "posts"; }

    static xp::Schema schema() {
        return {
            {"id", xp::FieldType::Serial, xp::FieldOption::PrimaryKey},
            {"title", xp::FieldType::Text, xp::FieldOption::NotNull},
            {"authorId", xp::FieldType::Integer, xp::FieldOption::NotNull}
        };
    }

    static drogon::Task<Json::Value> findById(int64_t val) {
        Json::Value where;
        where["id"] = val;
        co_return co_await findUnique(where);
    }

};

struct UserClient {
    drogon::Task<void> create(const Json::Value& data) const {
        co_await ::User::create(data);
    }

    drogon::Task<Json::Value> findUnique(const Json::Value& query) const {
        Json::Value where = query;
        Json::Value include;
        if (query.isObject()) {
            if (query.isMember("where")) {
                where = query["where"];
            } else if (query.isMember("include")) {
                where = query;
                where.removeMember("include");
            }
            if (query.isMember("include")) {
                include = query["include"];
            }
        }
        Json::Value result = co_await ::User::findUnique(where);
        if (result.isNull() || include.isNull() || !include.isObject()) {
            co_return result;
        }
        if (include.isMember("posts") && include["posts"].asBool()) {
            if (result.isMember("id") && !result["id"].isNull()) {
                Json::Value subWhere;
                subWhere["authorId"] = result["id"];
                result["posts"] = co_await ::Post::findMany(subWhere);
            } else {
                result["posts"] = Json::Value(Json::arrayValue);
            }
        }
        co_return result;
    }

    drogon::Task<Json::Value> findMany(const Json::Value& query = Json::Value()) const {
        Json::Value where = query;
        Json::Value include;
        if (query.isObject()) {
            if (query.isMember("where")) {
                where = query["where"];
            } else if (query.isMember("include")) {
                where = query;
                where.removeMember("include");
            }
            if (query.isMember("include")) {
                include = query["include"];
            }
        }
        Json::Value results = co_await ::User::findMany(where);
        if (results.isNull() || !results.isArray() || results.empty() || include.isNull() || !include.isObject()) {
            co_return results;
        }
        for (auto& result : results) {
            if (include.isMember("posts") && include["posts"].asBool()) {
                if (result.isMember("id") && !result["id"].isNull()) {
                    Json::Value subWhere;
                    subWhere["authorId"] = result["id"];
                    result["posts"] = co_await ::Post::findMany(subWhere);
                } else {
                    result["posts"] = Json::Value(Json::arrayValue);
                }
            }
        }
        co_return results;
    }

    drogon::Task<Json::Value> findFirst(const Json::Value& query) const {
        co_return co_await findUnique(query);
    }

    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) const {
        co_await ::User::update(where, data);
    }

    drogon::Task<void> deleteMany(const Json::Value& where) const {
        co_await ::User::deleteMany(where);
    }

    drogon::Task<Json::Value> findById(int64_t val) const {
        co_return co_await ::User::findById(val);
    }

    drogon::Task<Json::Value> findByUsername(const std::string& val) const {
        co_return co_await ::User::findByUsername(val);
    }

};

struct PostClient {
    drogon::Task<void> create(const Json::Value& data) const {
        co_await ::Post::create(data);
    }

    drogon::Task<Json::Value> findUnique(const Json::Value& query) const {
        Json::Value where = query;
        Json::Value include;
        if (query.isObject()) {
            if (query.isMember("where")) {
                where = query["where"];
            } else if (query.isMember("include")) {
                where = query;
                where.removeMember("include");
            }
            if (query.isMember("include")) {
                include = query["include"];
            }
        }
        Json::Value result = co_await ::Post::findUnique(where);
        if (result.isNull() || include.isNull() || !include.isObject()) {
            co_return result;
        }
        if (include.isMember("author") && include["author"].asBool()) {
            if (result.isMember("authorId") && !result["authorId"].isNull()) {
                Json::Value subWhere;
                subWhere["id"] = result["authorId"];
                result["author"] = co_await ::User::findUnique(subWhere);
            } else {
                result["author"] = Json::Value(Json::nullValue);
            }
        }
        co_return result;
    }

    drogon::Task<Json::Value> findMany(const Json::Value& query = Json::Value()) const {
        Json::Value where = query;
        Json::Value include;
        if (query.isObject()) {
            if (query.isMember("where")) {
                where = query["where"];
            } else if (query.isMember("include")) {
                where = query;
                where.removeMember("include");
            }
            if (query.isMember("include")) {
                include = query["include"];
            }
        }
        Json::Value results = co_await ::Post::findMany(where);
        if (results.isNull() || !results.isArray() || results.empty() || include.isNull() || !include.isObject()) {
            co_return results;
        }
        for (auto& result : results) {
            if (include.isMember("author") && include["author"].asBool()) {
                if (result.isMember("authorId") && !result["authorId"].isNull()) {
                    Json::Value subWhere;
                    subWhere["id"] = result["authorId"];
                    result["author"] = co_await ::User::findUnique(subWhere);
                } else {
                    result["author"] = Json::Value(Json::nullValue);
                }
            }
        }
        co_return results;
    }

    drogon::Task<Json::Value> findFirst(const Json::Value& query) const {
        co_return co_await findUnique(query);
    }

    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) const {
        co_await ::Post::update(where, data);
    }

    drogon::Task<void> deleteMany(const Json::Value& where) const {
        co_await ::Post::deleteMany(where);
    }

    drogon::Task<Json::Value> findById(int64_t val) const {
        co_return co_await ::Post::findById(val);
    }

};

struct PrismaClient {
    UserClient user;
    PostClient post;
};

inline constexpr PrismaClient prisma{};

class SchemaSync {
public:
    static drogon::Task<void> syncAll() {
        co_await ::User::sync();
        co_await ::Post::sync();
        co_return;
    }
};
