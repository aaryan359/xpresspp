#include "cli/schema_ir.h"

#include <fstream>
#include <optional>
#include <stdexcept>

namespace xp::cli {
namespace {

std::string escapeJson(std::string_view value) {
    std::string result;
    result.reserve(value.size());
    for (const char c : value) {
        switch (c) {
        case '\\': result += "\\\\"; break;
        case '"': result += "\\\""; break;
        case '\n': result += "\\n"; break;
        case '\r': result += "\\r"; break;
        case '\t': result += "\\t"; break;
        default: result += c; break;
        }
    }
    return result;
}

const char* kindName(ScalarKind kind) {
    switch (kind) {
    case ScalarKind::Serial: return "serial";
    case ScalarKind::Int: return "int";
    case ScalarKind::String: return "string";
    case ScalarKind::Boolean: return "boolean";
    case ScalarKind::Float: return "float";
    case ScalarKind::DateTime: return "datetime";
    case ScalarKind::Relation: return "relation";
    }
    return "string";
}

std::optional<std::string> jsonString(std::string_view line, std::string_view key) {
    const std::string marker = "\"" + std::string(key) + "\": \"";
    auto position = line.find(marker);
    if (position == std::string_view::npos) return std::nullopt;
    position += marker.size();
    std::string value;
    bool escaped = false;
    for (; position < line.size(); ++position) {
        const char c = line[position];
        if (escaped) {
            switch (c) {
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            default: value += c; break;
            }
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (c == '"') {
            return value;
        } else {
            value += c;
        }
    }
    return std::nullopt;
}

bool jsonBool(std::string_view line, std::string_view key) {
    const std::string marker = "\"" + std::string(key) + "\": ";
    const auto position = line.find(marker);
    return position != std::string_view::npos && line.substr(position + marker.size(), 4) == "true";
}

} // namespace

ScalarKind scalarKind(std::string_view type, bool relation) {
    if (relation) return ScalarKind::Relation;
    if (type == "Serial") return ScalarKind::Serial;
    if (type == "Int") return ScalarKind::Int;
    if (type == "Boolean") return ScalarKind::Boolean;
    if (type == "Float") return ScalarKind::Float;
    if (type == "DateTime") return ScalarKind::DateTime;
    return ScalarKind::String;
}

void writeSchemaJson(const SchemaIR& schema, const std::string& path) {
    std::ofstream out(path, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write schema IR: " + path);
    out << "{\n  \"formatVersion\": " << schema.formatVersion
        << ",\n  \"provider\": \"" << escapeJson(schema.provider) << "\",\n  \"models\": [\n";
    for (std::size_t mi = 0; mi < schema.models.size(); ++mi) {
        const auto& model = schema.models[mi];
        out << "    {\"name\": \"" << escapeJson(model.name)
            << "\", \"table\": \"" << escapeJson(model.tableName) << "\", \"fields\": [\n";
        for (std::size_t fi = 0; fi < model.fields.size(); ++fi) {
            const auto& field = model.fields[fi];
            out << "      {\"name\": \"" << escapeJson(field.name)
                << "\", \"type\": \"" << escapeJson(field.type)
                << "\", \"kind\": \"" << kindName(field.kind)
                << "\", \"nullable\": " << (field.nullable ? "true" : "false")
                << ", \"primaryKey\": " << (field.isPrimaryKey ? "true" : "false")
                << ", \"unique\": " << (field.isUnique ? "true" : "false")
                << ", \"defaultNow\": " << (field.isDefaultNow ? "true" : "false")
                << ", \"relation\": " << (field.isRelation ? "true" : "false")
                << ", \"list\": " << (field.isList ? "true" : "false")
                << ", \"relationModel\": \"" << escapeJson(field.relationModel)
                << "\", \"relationFields\": \"" << escapeJson(field.relationFields)
                << "\", \"relationReferences\": \"" << escapeJson(field.relationReferences) << "\"}";
            out << (fi + 1 == model.fields.size() ? "\n" : ",\n");
        }
        out << "    ]}" << (mi + 1 == schema.models.size() ? "\n" : ",\n");
    }
    out << "  ]\n}\n";
}

std::optional<SchemaIR> readSchemaJson(const std::string& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return std::nullopt;

    SchemaIR schema;
    ModelIR* current = nullptr;
    std::string line;
    while (std::getline(input, line)) {
        if (auto provider = jsonString(line, "provider")) schema.provider = *provider;
        if (line.find("\"table\":") != std::string::npos && line.find("\"fields\":") != std::string::npos) {
            auto name = jsonString(line, "name");
            auto table = jsonString(line, "table");
            if (!name || !table) return std::nullopt;
            schema.models.push_back(ModelIR{*name, *table, {}});
            current = &schema.models.back();
        } else if (current && line.find("\"kind\":") != std::string::npos) {
            FieldIR field;
            auto name = jsonString(line, "name");
            auto type = jsonString(line, "type");
            if (!name || !type) return std::nullopt;
            field.name = *name;
            field.type = *type;
            field.nullable = jsonBool(line, "nullable");
            field.isPrimaryKey = jsonBool(line, "primaryKey");
            field.isUnique = jsonBool(line, "unique");
            field.isDefaultNow = jsonBool(line, "defaultNow");
            field.isRelation = jsonBool(line, "relation");
            field.isList = jsonBool(line, "list");
            field.relationModel = jsonString(line, "relationModel").value_or("");
            field.relationFields = jsonString(line, "relationFields").value_or("");
            field.relationReferences = jsonString(line, "relationReferences").value_or("");
            field.kind = scalarKind(field.type, field.isRelation);
            current->fields.push_back(std::move(field));
        }
    }
    if (schema.models.empty()) return std::nullopt;
    return schema;
}

} // namespace xp::cli
