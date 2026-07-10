#include "cli/db_generator.h"

#include <cctype>
#include <fstream>
#include <stdexcept>

namespace xp::cli {
namespace {

std::string camel(std::string value) {
    if (!value.empty()) value.front() = static_cast<char>(std::tolower(static_cast<unsigned char>(value.front())));
    return value;
}

std::string upperFirst(std::string value) {
    if (!value.empty()) value.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(value.front())));
    return value;
}

std::string baseType(const FieldIR& field) {
    switch (field.kind) {
    case ScalarKind::Serial:
    case ScalarKind::Int: return "std::int64_t";
    case ScalarKind::Boolean: return "bool";
    case ScalarKind::Float: return "double";
    case ScalarKind::DateTime:
    case ScalarKind::String: return "std::string";
    case ScalarKind::Relation: break;
    }
    throw std::logic_error("A relation cannot be emitted as a scalar");
}

std::string cppType(const FieldIR& field) {
    return field.nullable ? "std::optional<" + baseType(field) + ">" : baseType(field);
}

std::string jsonRead(const FieldIR& field, std::string_view object) {
    const std::string access = std::string(object) + "[\"" + field.name + "\"]";
    switch (field.kind) {
    case ScalarKind::Serial:
    case ScalarKind::Int: return access + ".asInt64()";
    case ScalarKind::Boolean: return access + ".asBool()";
    case ScalarKind::Float: return access + ".asDouble()";
    case ScalarKind::DateTime:
    case ScalarKind::String: return access + ".asString()";
    case ScalarKind::Relation: break;
    }
    return {};
}

std::string placeholder(const SchemaIR& schema, std::size_t index) {
    return schema.provider == "postgres" || schema.provider == "postgresql"
        ? "$" + std::to_string(index) : "?";
}

std::vector<const FieldIR*> scalarFields(const ModelIR& model) {
    std::vector<const FieldIR*> result;
    for (const auto& field : model.fields) if (!field.isRelation) result.push_back(&field);
    return result;
}

std::vector<const FieldIR*> createFields(const ModelIR& model) {
    std::vector<const FieldIR*> result;
    for (const auto& field : model.fields)
        if (!field.isRelation && field.kind != ScalarKind::Serial && !field.isDefaultNow)
            result.push_back(&field);
    return result;
}

const FieldIR* primaryKey(const ModelIR& model) {
    for (const auto& field : model.fields)
        if (!field.isRelation && field.isPrimaryKey) return &field;
    return nullptr;
}

void emitRelationForwards(std::ofstream& out, const SchemaIR& schema) {
    for (const auto& model : schema.models) for (const auto& field : model.fields) {
        if (!field.isRelation) continue;
        out << "struct " << model.name << upperFirst(field.name) << "Include;\n"
            << "struct " << model.name << "With" << upperFirst(field.name) << ";\n"
            << "class " << model.name << upperFirst(field.name) << "Query;\n";
    }
    out << "\n";
}

void emitModel(std::ofstream& out, const SchemaIR& schema, const ModelIR& model) {
    const auto fields = scalarFields(model);
    const auto writable = createFields(model);
    const auto* id = primaryKey(model);

    out << "struct " << model.name << " {\n";
    for (const auto* field : fields) out << "    " << cppType(*field) << " " << field->name << "{};\n";
    out << "\n    Json::Value toJson() const {\n        Json::Value value(Json::objectValue);\n";
    for (const auto* field : fields) {
        if (field->nullable)
            out << "        if (" << field->name << ") value[\"" << field->name << "\"] = *" << field->name
                << "; else value[\"" << field->name << "\"] = Json::nullValue;\n";
        else
            out << "        value[\"" << field->name << "\"] = " << field->name << ";\n";
    }
    out << "        return value;\n    }\n\n"
        << "    static " << model.name << " fromRow(const drogon::orm::Row& row) {\n"
        << "        " << model.name << " value;\n";
    for (const auto* field : fields) {
        if (field->nullable)
            out << "        if (!row[\"" << field->name << "\"].isNull()) value." << field->name
                << " = row[\"" << field->name << "\"].as<" << baseType(*field) << ">();\n";
        else
            out << "        value." << field->name << " = row[\"" << field->name << "\"].as<"
                << baseType(*field) << ">();\n";
    }
    out << "        return value;\n    }\n};\n\n";

    out << "struct " << model.name << "Create {\n";
    for (const auto* field : writable) out << "    " << cppType(*field) << " " << field->name << "{};\n";
    out << "\n    static " << model.name << "Create fromJson(const Json::Value& value) {\n"
        << "        if (!value.isObject()) throw std::invalid_argument(\"" << model.name << " create data must be an object\");\n"
        << "        " << model.name << "Create data;\n";
    for (const auto* field : writable) {
        if (!field->nullable) {
            out << "        if (!value.isMember(\"" << field->name << "\") || value[\"" << field->name
                << "\"].isNull()) throw std::invalid_argument(\"Missing required field: " << field->name << "\");\n"
                << "        data." << field->name << " = " << jsonRead(*field, "value") << ";\n";
        } else {
            out << "        if (value.isMember(\"" << field->name << "\") && !value[\"" << field->name
                << "\"].isNull()) data." << field->name << " = " << jsonRead(*field, "value") << ";\n";
        }
    }
    out << "        return data;\n    }\n};\n\n";

    out << "struct " << model.name << "Update {\n";
    for (const auto* field : writable)
        out << "    xp::data::Patch<" << cppType(*field) << "> " << field->name << ";\n";
    out << "};\n\n"
        << "struct " << model.name << "Where {\n"
        << "    std::optional<xp::data::Expression<" << model.name << ">> expression;\n"
        << "};\n\n"
        << "struct " << model.name << "Columns {\n";
    for (std::size_t i = 0; i < fields.size(); ++i)
        out << "    static inline constexpr xp::data::Column<" << model.name << ", " << cppType(*fields[i])
            << "> " << fields[i]->name << "{" << i << "};\n";
    out << "};\n\n";

    out << "class " << model.name << "Client {\npublic:\n"
        << "    " << model.name << "Client(xp::data::TransactionContext* transaction = nullptr) : transaction_(transaction) {}\n"
        << "    xp::data::QueryBuilder<" << model.name << ", " << model.name << "Client> query() const { return xp::data::QueryBuilder<"
        << model.name << ", " << model.name << "Client>(*this); }\n"
        << "    xp::data::TransactionContext* transaction() const { return transaction_; }\n";
    for (const auto& relation : model.fields) if (relation.isRelation) {
        const auto prefix = model.name + upperFirst(relation.name);
        out << "    " << prefix << "Query with" << upperFirst(relation.name) << "() const;\n"
            << "    " << prefix << "Query with" << upperFirst(relation.name) << "(" << prefix << "Include options) const;\n";
    }
    out << "\n";

    out << "    drogon::Task<" << model.name << "> create(const " << model.name << "Create& data) const {\n"
        << "        static constexpr std::string_view sql = \"INSERT INTO " << model.tableName << " (";
    for (std::size_t i = 0; i < writable.size(); ++i) out << (i ? ", " : "") << writable[i]->name;
    out << ") VALUES (";
    for (std::size_t i = 0; i < writable.size(); ++i) out << (i ? ", " : "") << placeholder(schema, i + 1);
    out << ") RETURNING ";
    for (std::size_t i = 0; i < fields.size(); ++i) out << (i ? ", " : "") << fields[i]->name;
    out << "\";\n        std::vector<xp::QueryParam> params;\n";
    for (const auto* field : writable) {
        if (field->nullable)
            out << "        params.push_back(data." << field->name << " ? xp::data::bindValue(*data." << field->name
                << ") : xp::QueryParam(nullptr));\n";
        else
            out << "        params.push_back(xp::data::bindValue(data." << field->name << "));\n";
    }
    out << "        const auto result = co_await xp::executeParameterized(client(), std::string(sql), params);\n"
        << "        if (result.empty()) throw std::runtime_error(\"Create did not return a row\");\n"
        << "        co_return " << model.name << "::fromRow(result.front());\n    }\n\n"
        << "    drogon::Task<" << model.name << "> create(const Json::Value& value) const { co_return co_await create("
        << model.name << "Create::fromJson(value)); }\n\n"
        << "    drogon::Task<std::vector<" << model.name << ">> findMany() const { co_return co_await executeMany({}); }\n"
        << "    drogon::Task<std::vector<" << model.name << ">> findMany(const " << model.name << "Where& where) const {\n"
        << "        xp::data::QuerySpec<" << model.name << "> spec; spec.where = where.expression; co_return co_await executeMany(spec);\n    }\n\n";

    if (id) {
        out << "    drogon::Task<std::optional<" << model.name << ">> findById(" << baseType(*id) << " id) const {\n"
            << "        co_return co_await query().where(" << model.name << "Columns::" << id->name << " == id).one();\n    }\n\n";
    }
    for (const auto* field : fields) if ((field->isUnique || field->isPrimaryKey) && field != id)
        out << "    drogon::Task<std::optional<" << model.name << ">> findBy" << upperFirst(field->name)
            << "(" << baseType(*field) << " value) const { co_return co_await query().where("
            << model.name << "Columns::" << field->name << " == value).one(); }\n";

    out << "\n    [[deprecated(\"Use typed findBy... or query().where()\")]]\n"
        << "    drogon::Task<Json::Value> findUnique(const Json::Value& input) const {\n"
        << "        const auto& where = input.isMember(\"where\") ? input[\"where\"] : input;\n"
        << "        if (!where.isObject() || where.size() != 1) throw std::invalid_argument(\"findUnique requires exactly one generated unique field\");\n";
    bool firstUnique = true;
    for (const auto* field : fields) if (field->isUnique || field->isPrimaryKey) {
        out << "        " << (firstUnique ? "if" : "else if") << " (where.isMember(\"" << field->name
            << "\")) { auto result = co_await findBy" << upperFirst(field->name) << "(" << jsonRead(*field, "where")
            << "); co_return result ? result->toJson() : Json::Value(Json::nullValue); }\n";
        firstUnique = false;
    }
    out << "        throw std::invalid_argument(\"Unknown or non-unique field in findUnique\");\n    }\n\n";

    if (id) {
        out << "    drogon::Task<" << model.name << "> updateById(" << baseType(*id) << " id, const "
            << model.name << "Update& patch) const {\n"
            << "        static constexpr std::string_view sql = \"UPDATE " << model.tableName << " SET ";
        std::size_t param = 1;
        for (std::size_t i = 0; i < writable.size(); ++i) {
            if (i) out << ", ";
            out << writable[i]->name << " = CASE WHEN " << placeholder(schema, param++) << " THEN "
                << placeholder(schema, param++) << " ELSE " << writable[i]->name << " END";
        }
        out << " WHERE " << id->name << " = " << placeholder(schema, param) << " RETURNING ";
        for (std::size_t i = 0; i < fields.size(); ++i) out << (i ? ", " : "") << fields[i]->name;
        out << "\";\n        std::vector<xp::QueryParam> params;\n";
        for (const auto* field : writable) {
            out << "        params.push_back(patch." << field->name << ".present());\n"
                << "        if (!patch." << field->name << ".present()) params.push_back(nullptr); else ";
            if (field->nullable)
                out << "params.push_back(patch." << field->name << ".value() ? xp::data::bindValue(*patch."
                    << field->name << ".value()) : xp::QueryParam(nullptr));\n";
            else
                out << "params.push_back(xp::data::bindValue(patch." << field->name << ".value()));\n";
        }
        out << "        params.push_back(xp::data::bindValue(id));\n"
            << "        const auto rows = co_await xp::executeParameterized(client(), std::string(sql), params);\n"
            << "        if (rows.empty()) throw std::out_of_range(\"" << model.name << " was not found\");\n"
            << "        co_return " << model.name << "::fromRow(rows.front());\n    }\n\n"
            << "    drogon::Task<bool> deleteById(" << baseType(*id) << " id) const {\n"
            << "        static constexpr std::string_view sql = \"DELETE FROM " << model.tableName << " WHERE "
            << id->name << " = " << placeholder(schema, 1) << "\";\n"
            << "        std::vector<xp::QueryParam> params; params.push_back(xp::data::bindValue(id));\n"
            << "        const auto rows = co_await xp::executeParameterized(client(), std::string(sql), params);\n"
            << "        co_return rows.affectedRows() != 0;\n    }\n\n";
    }

    out << "    drogon::Task<std::vector<" << model.name << ">> executeMany(const xp::data::QuerySpec<"
        << model.name << ">& spec) const {\n"
        << "        const auto rendered = renderer().select(\"" << model.tableName << "\", \"";
    for (std::size_t i = 0; i < fields.size(); ++i) out << (i ? ", " : "") << fields[i]->name;
    out << "\", spec);\n"
        << "        const auto rows = co_await xp::executeParameterized(client(), rendered.sql, rendered.parameters);\n"
        << "        std::vector<" << model.name << "> values; values.reserve(rows.size());\n"
        << "        for (const auto& row : rows) values.push_back(" << model.name << "::fromRow(row));\n"
        << "        co_return values;\n    }\n\n"
        << "    drogon::Task<std::optional<" << model.name << ">> executeOne(xp::data::QuerySpec<" << model.name << "> spec) const {\n"
        << "        spec.limit = 1; auto values = co_await executeMany(spec); if (values.empty()) co_return std::nullopt; co_return values.front();\n    }\n\n"
        << "    drogon::Task<std::size_t> executeDelete(const xp::data::QuerySpec<" << model.name << ">& spec, bool allowAll) const {\n"
        << "        const auto rendered = renderer().remove(\"" << model.tableName << "\", spec, allowAll);\n"
        << "        const auto rows = co_await xp::executeParameterized(client(), rendered.sql, rendered.parameters); co_return rows.affectedRows();\n    }\n\n"
        << "private:\n"
        << "    drogon::orm::DbClientPtr client() const {\n"
        << "        if (transaction_) return transaction_->client(); auto value = drogon::app().getDbClient(\"default\");\n"
        << "        if (!value) throw std::runtime_error(\"Database is not connected\"); return value;\n    }\n"
        << "    static xp::data::PostgreSqlRenderer<" << model.name << "> renderer() { return xp::data::PostgreSqlRenderer<"
        << model.name << ">([](std::size_t field) -> std::string_view {\n"
        << "        static constexpr std::string_view fields[] = {";
    for (std::size_t i = 0; i < fields.size(); ++i) out << (i ? ", " : "") << "\"" << fields[i]->name << "\"";
    out << "}; return field < std::size(fields) ? fields[field] : std::string_view{}; }); }\n"
        << "    xp::data::TransactionContext* transaction_;\n};\n\n";
}

const ModelIR* findModel(const SchemaIR& schema, const std::string& name) {
    for (const auto& model : schema.models) if (model.name == name) return &model;
    return nullptr;
}

const FieldIR* findField(const ModelIR& model, const std::string& name) {
    for (const auto& field : model.fields) if (field.name == name) return &field;
    return nullptr;
}

void emitRelations(std::ofstream& out, const SchemaIR& schema) {
    for (const auto& source : schema.models) for (const auto& relation : source.fields) {
        if (!relation.isRelation) continue;
        const auto* target = findModel(schema, relation.relationModel);
        if (!target) throw std::runtime_error("Unknown relation model: " + relation.relationModel);
        const auto* sourceKey = findField(source, relation.relationFields);
        const auto* targetKey = findField(*target, relation.relationReferences);
        if (!sourceKey || !targetKey)
            throw std::runtime_error("Relation " + source.name + "." + relation.name + " is missing valid fields/references");

        const auto prefix = source.name + upperFirst(relation.name);
        out << "struct " << prefix << "Include {\n"
            << "    std::optional<xp::data::Expression<" << target->name << ">> where;\n"
            << "    std::vector<xp::data::Order<" << target->name << ">> orderBy;\n"
            << "    std::optional<std::size_t> limitPerParent;\n"
            << "};\n\n"
            << "struct " << source.name << "With" << upperFirst(relation.name) << " {\n"
            << "    " << source.name << " value;\n"
            << "    " << (relation.isList ? "std::vector<" + target->name + ">" : "std::optional<" + target->name + ">")
            << " " << relation.name << ";\n"
            << "    Json::Value toJson() const { auto json = value.toJson();\n";
        if (relation.isList)
            out << "        json[\"" << relation.name << "\"] = Json::arrayValue; for (const auto& item : "
                << relation.name << ") json[\"" << relation.name << "\"].append(item.toJson());\n";
        else
            out << "        json[\"" << relation.name << "\"] = " << relation.name << " ? "
                << relation.name << "->toJson() : Json::Value(Json::nullValue);\n";
        out << "        return json; }\n};\n\n"
            << "class " << prefix << "Query {\npublic:\n"
            << "    " << prefix << "Query(" << source.name << "Client source, " << prefix
            << "Include options = {}) : source_(std::move(source)), options_(std::move(options)) {}\n"
            << "    drogon::Task<std::vector<" << source.name << "With" << upperFirst(relation.name)
            << ">> findMany() const {\n"
            << "        auto parents = co_await source_.findMany();\n"
            << "        std::vector<" << baseType(*sourceKey) << "> keys; keys.reserve(parents.size());\n"
            << "        for (const auto& parent : parents) ";
        if (sourceKey->nullable)
            out << "if (parent." << sourceKey->name << ") keys.push_back(*parent." << sourceKey->name << ");\n";
        else
            out << "keys.push_back(parent." << sourceKey->name << ");\n";
        out << "        std::vector<" << source.name << "With" << upperFirst(relation.name)
            << "> result; result.reserve(parents.size());\n"
            << "        if (keys.empty()) { for (auto& parent : parents) result.push_back({std::move(parent), {}}); co_return result; }\n"
            << "        " << target->name << "Client target(source_.transaction());\n"
            << "        auto expression = " << target->name << "Columns::" << targetKey->name << ".in(keys);\n"
            << "        if (options_.where) expression = expression && *options_.where;\n"
            << "        auto query = target.query().where(std::move(expression));\n"
            << "        for (const auto& order : options_.orderBy) query.orderBy(order);\n"
            << "        auto children = co_await query.all();\n";
        if (relation.isList) {
            out << "        std::unordered_map<" << baseType(*targetKey) << ", std::vector<" << target->name << ">> grouped;\n"
                << "        for (auto& child : children) {\n";
            if (targetKey->nullable)
                out << "            if (child." << targetKey->name << ") grouped[*child." << targetKey->name
                    << "].push_back(std::move(child));\n";
            else
                out << "            grouped[child." << targetKey->name << "].push_back(std::move(child));\n";
            out << "        }\n"
                << "        for (auto& parent : parents) { std::vector<" << target->name << "> items;\n"
                << "            auto found = grouped.find(" << (sourceKey->nullable ? "*parent." : "parent.")
                << sourceKey->name << "); if (found != grouped.end()) items = std::move(found->second);\n"
                << "            if (options_.limitPerParent && items.size() > *options_.limitPerParent) items.resize(*options_.limitPerParent);\n"
                << "            result.push_back({std::move(parent), std::move(items)}); }\n";
        } else {
            out << "        std::unordered_map<" << baseType(*targetKey) << ", " << target->name << "> grouped;\n"
                << "        for (auto& child : children) ";
            if (targetKey->nullable)
                out << "if (child." << targetKey->name << ") grouped.emplace(*child." << targetKey->name << ", std::move(child));\n";
            else
                out << "grouped.emplace(child." << targetKey->name << ", std::move(child));\n";
            out << "        for (auto& parent : parents) { std::optional<" << target->name << "> item;\n"
                << "            auto found = grouped.find(" << (sourceKey->nullable ? "*parent." : "parent.")
                << sourceKey->name << "); if (found != grouped.end()) item = std::move(found->second);\n"
                << "            result.push_back({std::move(parent), std::move(item)}); }\n";
        }
        out << "        co_return result;\n    }\n"
            << "private:\n    " << source.name << "Client source_;\n    " << prefix << "Include options_;\n};\n\n"
            << "inline " << prefix << "Query " << source.name << "Client::with" << upperFirst(relation.name)
            << "() const { return " << prefix << "Query(*this); }\n"
            << "inline " << prefix << "Query " << source.name << "Client::with" << upperFirst(relation.name)
            << "(" << prefix << "Include options) const { return " << prefix << "Query(*this, std::move(options)); }\n\n";
    }
}

} // namespace

void generateDatabaseHeader(const SchemaIR& schema, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write generated database header: " + path);
    out << "#pragma once\n#include <xpresspp/xpresspp.h>\n#include <cstdint>\n#include <optional>\n"
        << "#include <string>\n#include <type_traits>\n#include <unordered_map>\n#include <utility>\n#include <vector>\n\n";
    for (const auto& model : schema.models) out << "struct " << model.name << ";\nclass " << model.name << "Client;\n";
    emitRelationForwards(out, schema);
    for (const auto& model : schema.models) emitModel(out, schema, model);
    emitRelations(out, schema);

    out << "struct TransactionClient {\n";
    for (const auto& model : schema.models) out << "    " << model.name << "Client " << camel(model.name) << ";\n";
    out << "    explicit TransactionClient(xp::data::TransactionContext& transaction)";
    for (std::size_t i = 0; i < schema.models.size(); ++i)
        out << (i ? ", " : " : ") << camel(schema.models[i].name) << "(&transaction)";
    out << " {}\n"
        << "    TransactionClient(const TransactionClient&) = delete;\n"
        << "    TransactionClient& operator=(const TransactionClient&) = delete;\n"
        << "    TransactionClient(TransactionClient&&) = delete;\n"
        << "    TransactionClient& operator=(TransactionClient&&) = delete;\n"
        << "};\n\n"
        << "struct XpdClient {\n";
    for (const auto& model : schema.models) out << "    " << model.name << "Client " << camel(model.name) << ";\n";
    out << "\n    template <typename Work>\n"
        << "    auto transaction(Work&& work) const -> drogon::Task<xp::data::TaskValueT<std::invoke_result_t<Work, TransactionClient&>>> {\n"
        << "        auto database = drogon::app().getDbClient(\"default\"); if (!database) throw std::runtime_error(\"Database is not connected\");\n"
        << "        return xp::data::runTransaction<TransactionClient>(database, [](xp::data::TransactionContext& context) { return TransactionClient(context); }, std::forward<Work>(work));\n"
        << "    }\n\n"
        << "    template <typename Model> drogon::Task<std::vector<Model>> rawMany(std::string sql, std::vector<xp::QueryParam> params = {}) const {\n"
        << "        auto database = drogon::app().getDbClient(\"default\"); if (!database) throw std::runtime_error(\"Database is not connected\");\n"
        << "        const auto rows = co_await xp::executeParameterized(database, sql, params); std::vector<Model> values; values.reserve(rows.size());\n"
        << "        for (const auto& row : rows) values.push_back(Model::fromRow(row)); co_return values;\n    }\n};\n\n"
        << "inline XpdClient xpd{};\n\n"
        << "class SchemaSync { public: static drogon::Task<void> syncAll() { co_await xp::DatabaseManager::instance().runMigrations(); } };\n"
        << "struct AutoSyncRegister { AutoSyncRegister() { xp::DatabaseManager::instance().registerSync([]() -> drogon::Task<void> { co_await SchemaSync::syncAll(); }); } };\n"
        << "inline AutoSyncRegister auto_sync_register_instance;\n";
}

} // namespace xp::cli
