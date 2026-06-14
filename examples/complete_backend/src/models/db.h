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
            {"name", xp::FieldType::Text, xp::FieldOption::None},
            {"username", xp::FieldType::Text, xp::FieldOption::Unique},
            {"email", xp::FieldType::Text, xp::FieldOption::NotNull | xp::FieldOption::Unique},
            {"profilePicture", xp::FieldType::Text, xp::FieldOption::None},
            {"password", xp::FieldType::Text, xp::FieldOption::None},
            {"Subscribed", xp::FieldType::Boolean, xp::FieldOption::None},
            {"created_at", xp::FieldType::Timestamp, xp::FieldOption::NotNull | xp::FieldOption::DefaultNow}
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

    static drogon::Task<Json::Value> findByEmail(const std::string& val) {
        Json::Value where;
        where["email"] = val;
        co_return co_await findUnique(where);
    }

    static drogon::Task<void> create(const std::string& username, const std::string& password) {
        Json::Value data;
        data["username"] = username;
        data["password"] = password;
        co_await xp::Model<User>::create(data);
    }

};

class MarketPrompt : public xp::Model<MarketPrompt> {
public:
    using xp::Model<MarketPrompt>::create;

    static std::string tableName() { return "market_prompts"; }

    static xp::Schema schema() {
        return {
            {"id", xp::FieldType::Serial, xp::FieldOption::PrimaryKey},
            {"title", xp::FieldType::Text, xp::FieldOption::NotNull},
            {"description", xp::FieldType::Text, xp::FieldOption::NotNull},
            {"category", xp::FieldType::Text, xp::FieldOption::NotNull},
            {"price", xp::FieldType::Double, xp::FieldOption::NotNull},
            {"rating", xp::FieldType::Double, xp::FieldOption::NotNull},
            {"likesCount", xp::FieldType::Integer, xp::FieldOption::NotNull},
            {"outputImage", xp::FieldType::Text, xp::FieldOption::NotNull},
            {"outputText", xp::FieldType::Text, xp::FieldOption::None},
            {"modelUsed", xp::FieldType::Text, xp::FieldOption::None},
            {"userPrompt", xp::FieldType::Text, xp::FieldOption::None},
            {"systemPrompt", xp::FieldType::Text, xp::FieldOption::None},
            {"authorId", xp::FieldType::Integer, xp::FieldOption::NotNull},
            {"isPurchased", xp::FieldType::Boolean, xp::FieldOption::None},
            {"createdAt", xp::FieldType::Timestamp, xp::FieldOption::NotNull | xp::FieldOption::DefaultNow}
        };
    }

    static drogon::Task<Json::Value> findById(int64_t val) {
        Json::Value where;
        where["id"] = val;
        co_return co_await findUnique(where);
    }

};

class ChatPrompt : public xp::Model<ChatPrompt> {
public:
    using xp::Model<ChatPrompt>::create;

    static std::string tableName() { return "chat_prompts"; }

    static xp::Schema schema() {
        return {
            {"id", xp::FieldType::Serial, xp::FieldOption::PrimaryKey},
            {"systemPrompt", xp::FieldType::Text, xp::FieldOption::None},
            {"userPrompt", xp::FieldType::Text, xp::FieldOption::NotNull},
            {"aiResponse", xp::FieldType::Text, xp::FieldOption::NotNull},
            {"voiceInput", xp::FieldType::Text, xp::FieldOption::None},
            {"userId", xp::FieldType::Integer, xp::FieldOption::NotNull},
            {"createdAt", xp::FieldType::Timestamp, xp::FieldOption::NotNull | xp::FieldOption::DefaultNow}
        };
    }

    static drogon::Task<Json::Value> findById(int64_t val) {
        Json::Value where;
        where["id"] = val;
        co_return co_await findUnique(where);
    }

};

class Like : public xp::Model<Like> {
public:
    using xp::Model<Like>::create;

    static std::string tableName() { return "likes"; }

    static xp::Schema schema() {
        return {
            {"id", xp::FieldType::Serial, xp::FieldOption::PrimaryKey},
            {"userId", xp::FieldType::Integer, xp::FieldOption::NotNull},
            {"promptId", xp::FieldType::Integer, xp::FieldOption::NotNull},
            {"createdAt", xp::FieldType::Timestamp, xp::FieldOption::NotNull | xp::FieldOption::DefaultNow}
        };
    }

    static drogon::Task<Json::Value> findById(int64_t val) {
        Json::Value where;
        where["id"] = val;
        co_return co_await findUnique(where);
    }

};

class PromptPurchase : public xp::Model<PromptPurchase> {
public:
    using xp::Model<PromptPurchase>::create;

    static std::string tableName() { return "prompt_purchases"; }

    static xp::Schema schema() {
        return {
            {"id", xp::FieldType::Serial, xp::FieldOption::PrimaryKey},
            {"userId", xp::FieldType::Integer, xp::FieldOption::NotNull},
            {"promptId", xp::FieldType::Integer, xp::FieldOption::NotNull},
            {"amount", xp::FieldType::Double, xp::FieldOption::NotNull},
            {"isActive", xp::FieldType::Boolean, xp::FieldOption::NotNull},
            {"paymentMethod", xp::FieldType::Text, xp::FieldOption::NotNull},
            {"purchasedAt", xp::FieldType::Timestamp, xp::FieldOption::NotNull | xp::FieldOption::DefaultNow}
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

    drogon::Task<void> create(const std::string& username, const std::string& password) const {
        co_await ::User::create(username, password);
    }

    drogon::Task<Json::Value> findUnique(const Json::Value& where) const {
        co_return co_await ::User::findUnique(where);
    }

    drogon::Task<Json::Value> findMany(const Json::Value& where = Json::Value()) const {
        co_return co_await ::User::findMany(where);
    }

    drogon::Task<Json::Value> findFirst(const Json::Value& where) const {
        co_return co_await ::User::findUnique(where);
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

    drogon::Task<Json::Value> findByEmail(const std::string& val) const {
        co_return co_await ::User::findByEmail(val);
    }

};

struct MarketPromptClient {
    drogon::Task<void> create(const Json::Value& data) const {
        co_await ::MarketPrompt::create(data);
    }

    drogon::Task<Json::Value> findUnique(const Json::Value& where) const {
        co_return co_await ::MarketPrompt::findUnique(where);
    }

    drogon::Task<Json::Value> findMany(const Json::Value& where = Json::Value()) const {
        co_return co_await ::MarketPrompt::findMany(where);
    }

    drogon::Task<Json::Value> findFirst(const Json::Value& where) const {
        co_return co_await ::MarketPrompt::findUnique(where);
    }

    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) const {
        co_await ::MarketPrompt::update(where, data);
    }

    drogon::Task<void> deleteMany(const Json::Value& where) const {
        co_await ::MarketPrompt::deleteMany(where);
    }

    drogon::Task<Json::Value> findById(int64_t val) const {
        co_return co_await ::MarketPrompt::findById(val);
    }

};

struct ChatPromptClient {
    drogon::Task<void> create(const Json::Value& data) const {
        co_await ::ChatPrompt::create(data);
    }

    drogon::Task<Json::Value> findUnique(const Json::Value& where) const {
        co_return co_await ::ChatPrompt::findUnique(where);
    }

    drogon::Task<Json::Value> findMany(const Json::Value& where = Json::Value()) const {
        co_return co_await ::ChatPrompt::findMany(where);
    }

    drogon::Task<Json::Value> findFirst(const Json::Value& where) const {
        co_return co_await ::ChatPrompt::findUnique(where);
    }

    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) const {
        co_await ::ChatPrompt::update(where, data);
    }

    drogon::Task<void> deleteMany(const Json::Value& where) const {
        co_await ::ChatPrompt::deleteMany(where);
    }

    drogon::Task<Json::Value> findById(int64_t val) const {
        co_return co_await ::ChatPrompt::findById(val);
    }

};

struct LikeClient {
    drogon::Task<void> create(const Json::Value& data) const {
        co_await ::Like::create(data);
    }

    drogon::Task<Json::Value> findUnique(const Json::Value& where) const {
        co_return co_await ::Like::findUnique(where);
    }

    drogon::Task<Json::Value> findMany(const Json::Value& where = Json::Value()) const {
        co_return co_await ::Like::findMany(where);
    }

    drogon::Task<Json::Value> findFirst(const Json::Value& where) const {
        co_return co_await ::Like::findUnique(where);
    }

    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) const {
        co_await ::Like::update(where, data);
    }

    drogon::Task<void> deleteMany(const Json::Value& where) const {
        co_await ::Like::deleteMany(where);
    }

    drogon::Task<Json::Value> findById(int64_t val) const {
        co_return co_await ::Like::findById(val);
    }

};

struct PromptPurchaseClient {
    drogon::Task<void> create(const Json::Value& data) const {
        co_await ::PromptPurchase::create(data);
    }

    drogon::Task<Json::Value> findUnique(const Json::Value& where) const {
        co_return co_await ::PromptPurchase::findUnique(where);
    }

    drogon::Task<Json::Value> findMany(const Json::Value& where = Json::Value()) const {
        co_return co_await ::PromptPurchase::findMany(where);
    }

    drogon::Task<Json::Value> findFirst(const Json::Value& where) const {
        co_return co_await ::PromptPurchase::findUnique(where);
    }

    drogon::Task<void> update(const Json::Value& where, const Json::Value& data) const {
        co_await ::PromptPurchase::update(where, data);
    }

    drogon::Task<void> deleteMany(const Json::Value& where) const {
        co_await ::PromptPurchase::deleteMany(where);
    }

    drogon::Task<Json::Value> findById(int64_t val) const {
        co_return co_await ::PromptPurchase::findById(val);
    }

};

struct PrismaClient {
    UserClient user;
    MarketPromptClient marketPrompt;
    ChatPromptClient chatPrompt;
    LikeClient like;
    PromptPurchaseClient promptPurchase;
};

inline constexpr PrismaClient prisma{};

class SchemaSync {
public:
    static drogon::Task<void> syncAll() {
        co_await ::User::sync();
        co_await ::MarketPrompt::sync();
        co_await ::ChatPrompt::sync();
        co_await ::Like::sync();
        co_await ::PromptPurchase::sync();
        co_return;
    }
};
