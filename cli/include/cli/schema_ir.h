#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <optional>

namespace xp::cli {

enum class ScalarKind { Serial, Int, String, Boolean, Float, DateTime, Relation };

struct FieldIR {
    std::string name;
    std::string type;
    ScalarKind kind = ScalarKind::String;
    bool nullable = false;
    bool isPrimaryKey = false;
    bool isUnique = false;
    bool isDefaultNow = false;
    bool isRelation = false;
    bool isList = false;
    std::string relationModel;
    std::string relationFields;
    std::string relationReferences;
};

struct ModelIR {
    std::string name;
    std::string tableName;
    std::vector<FieldIR> fields;
};

struct SchemaIR {
    unsigned formatVersion = 1;
    std::string provider = "postgresql";
    std::vector<ModelIR> models;
};

ScalarKind scalarKind(std::string_view type, bool relation);
void writeSchemaJson(const SchemaIR& schema, const std::string& path);
std::optional<SchemaIR> readSchemaJson(const std::string& path);

} // namespace xp::cli
