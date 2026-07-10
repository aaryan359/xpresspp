#pragma once

#include "query.h"

#include <functional>
#include <sstream>

namespace xp::data {

struct RenderedQuery {
    std::string sql;
    std::vector<QueryParam> parameters;
};

template <typename Model>
class PostgreSqlRenderer {
public:
    using FieldResolver = std::function<std::string_view(std::size_t)>;

    explicit PostgreSqlRenderer(FieldResolver resolver) : resolve_(std::move(resolver)) {}

    std::string where(const Expression<Model>& expression, std::vector<QueryParam>& parameters) const {
        return renderNode(expression.node(), parameters);
    }

    RenderedQuery select(std::string_view table, std::string_view fields,
                         const QuerySpec<Model>& spec) const {
        RenderedQuery query{"SELECT " + std::string(fields) + " FROM " + std::string(table), {}};
        appendSpec(query, spec);
        return query;
    }

    RenderedQuery remove(std::string_view table, const QuerySpec<Model>& spec,
                         bool allowAllRows = false) const {
        if (!spec.where && !allowAllRows)
            throw std::invalid_argument("Refusing DELETE without a where clause; pass xp::data::confirmAllRows explicitly");
        RenderedQuery query{"DELETE FROM " + std::string(table), {}};
        appendSpec(query, spec);
        return query;
    }

private:
    void appendSpec(RenderedQuery& query, const QuerySpec<Model>& spec) const {
        if (spec.where) query.sql += " WHERE " + where(*spec.where, query.parameters);
        if (!spec.orderBy.empty()) {
            query.sql += " ORDER BY ";
            for (std::size_t i = 0; i < spec.orderBy.size(); ++i) {
                if (i) query.sql += ", ";
                const auto field = resolve_(spec.orderBy[i].field);
                if (field.empty()) throw std::out_of_range("Invalid generated database field id");
                query.sql += std::string(field) +
                    (spec.orderBy[i].direction == SortDirection::Asc ? " ASC" : " DESC");
            }
        }
        if (spec.limit) query.sql += " LIMIT " + std::to_string(*spec.limit);
        if (spec.offset) query.sql += " OFFSET " + std::to_string(*spec.offset);
        if (spec.lockForUpdate) query.sql += " FOR UPDATE";
    }

    std::string placeholder(std::vector<QueryParam>& parameters, QueryParam value) const {
        parameters.push_back(std::move(value));
        return "$" + std::to_string(parameters.size());
    }

    std::string renderNode(const std::shared_ptr<ExpressionNode<Model>>& node,
                           std::vector<QueryParam>& parameters) const {
        if (!node) throw std::invalid_argument("Empty query expression");
        if (node->logic != LogicOp::Predicate) {
            const auto operation = node->logic == LogicOp::And ? " AND " : " OR ";
            const auto left = renderNode(node->left, parameters);
            const auto right = renderNode(node->right, parameters);
            return "(" + left + operation + right + ")";
        }

        const auto field = std::string(resolve_(node->field));
        if (field.empty()) throw std::out_of_range("Invalid generated database field id");
        if (node->comparison == CompareOp::IsNull) return field + " IS NULL";
        if (node->comparison == CompareOp::IsNotNull) return field + " IS NOT NULL";
        if (node->comparison == CompareOp::In) {
            std::string sql = field + " IN (";
            for (std::size_t i = 0; i < node->values.size(); ++i) {
                if (i) sql += ", ";
                sql += placeholder(parameters, node->values[i]);
            }
            return sql + ")";
        }

        static constexpr const char* operations[] = {"=", "!=", ">", ">=", "<", "<=", "LIKE"};
        const auto index = static_cast<std::size_t>(node->comparison);
        if (index >= std::size(operations) || node->values.size() != 1)
            throw std::invalid_argument("Invalid query comparison");
        return field + " " + operations[index] + " " + placeholder(parameters, node->values.front());
    }

    FieldResolver resolve_;
};

} // namespace xp::data
