#pragma once

#include "../database.h"

#include <cstddef>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace xp::data {

template <typename T>
class Patch {
public:
    Patch() = default;
    Patch(T value) : value_(std::move(value)), present_(true) {}

    Patch& operator=(T value) {
        value_ = std::move(value);
        present_ = true;
        return *this;
    }

    bool present() const { return present_; }
    const T& value() const {
        if (!present_) throw std::logic_error("Patch value was not provided");
        return value_;
    }

private:
    T value_{};
    bool present_ = false;
};

template <typename T>
class Patch<std::optional<T>> {
public:
    Patch() = default;
    Patch(std::optional<T> value) : value_(std::move(value)), present_(true) {}
    Patch(T value) : value_(std::move(value)), present_(true) {}
    Patch(std::nullptr_t) : value_(std::nullopt), present_(true) {}

    Patch& operator=(std::optional<T> value) {
        value_ = std::move(value);
        present_ = true;
        return *this;
    }
    Patch& operator=(T value) {
        value_ = std::move(value);
        present_ = true;
        return *this;
    }
    Patch& operator=(std::nullptr_t) { value_.reset(); present_ = true; return *this; }

    bool present() const { return present_; }
    const std::optional<T>& value() const {
        if (!present_) throw std::logic_error("Patch value was not provided");
        return value_;
    }

private:
    std::optional<T> value_;
    bool present_ = false;
};

enum class CompareOp { Eq, NotEq, Gt, Gte, Lt, Lte, Like, In, IsNull, IsNotNull };
enum class LogicOp { Predicate, And, Or };
enum class SortDirection { Asc, Desc };

template <typename Model>
struct ExpressionNode {
    LogicOp logic = LogicOp::Predicate;
    std::size_t field = 0;
    CompareOp comparison = CompareOp::Eq;
    std::vector<QueryParam> values;
    std::shared_ptr<ExpressionNode> left;
    std::shared_ptr<ExpressionNode> right;
};

template <typename T>
inline QueryParam bindValue(T&& value) {
    using V = std::remove_cvref_t<T>;
    if constexpr (std::is_same_v<V, std::string>) return value;
    else if constexpr (std::is_same_v<V, std::string_view>) return std::string(value);
    else if constexpr (std::is_same_v<V, const char*> || std::is_array_v<V>) return std::string(value);
    else if constexpr (std::is_same_v<V, bool>) return value;
    else if constexpr (std::is_floating_point_v<V>) return static_cast<double>(value);
    else if constexpr (std::is_integral_v<V> && sizeof(V) <= 4) return static_cast<int32_t>(value);
    else if constexpr (std::is_integral_v<V>) return static_cast<int64_t>(value);
    else static_assert(!sizeof(V), "Unsupported database bind value type");
}

template <typename Model>
class Expression {
public:
    using Node = ExpressionNode<Model>;

    Expression() = default;
    explicit Expression(Node node) : node_(std::make_shared<Node>(std::move(node))) {}

    const std::shared_ptr<Node>& node() const { return node_; }
    explicit operator bool() const { return static_cast<bool>(node_); }

    friend Expression operator&&(Expression left, Expression right) {
        Node node;
        node.logic = LogicOp::And;
        node.left = std::move(left.node_);
        node.right = std::move(right.node_);
        return Expression(std::move(node));
    }

    friend Expression operator||(Expression left, Expression right) {
        Node node;
        node.logic = LogicOp::Or;
        node.left = std::move(left.node_);
        node.right = std::move(right.node_);
        return Expression(std::move(node));
    }

private:
    std::shared_ptr<Node> node_;
};

template <typename Model>
struct Order {
    std::size_t field;
    SortDirection direction;
};

template <typename Model, typename T>
class Column {
public:
    constexpr explicit Column(std::size_t field) : field_(field) {}
    constexpr std::size_t id() const { return field_; }

    template <typename V> Expression<Model> operator==(V&& value) const { return compare(CompareOp::Eq, std::forward<V>(value)); }
    template <typename V> Expression<Model> operator!=(V&& value) const { return compare(CompareOp::NotEq, std::forward<V>(value)); }
    template <typename V> Expression<Model> operator>(V&& value) const { return compare(CompareOp::Gt, std::forward<V>(value)); }
    template <typename V> Expression<Model> operator>=(V&& value) const { return compare(CompareOp::Gte, std::forward<V>(value)); }
    template <typename V> Expression<Model> operator<(V&& value) const { return compare(CompareOp::Lt, std::forward<V>(value)); }
    template <typename V> Expression<Model> operator<=(V&& value) const { return compare(CompareOp::Lte, std::forward<V>(value)); }

    Expression<Model> contains(std::string_view value) const { return compare(CompareOp::Like, "%" + std::string(value) + "%"); }
    Expression<Model> startsWith(std::string_view value) const { return compare(CompareOp::Like, std::string(value) + "%"); }
    Expression<Model> endsWith(std::string_view value) const { return compare(CompareOp::Like, "%" + std::string(value)); }

    template <typename Range>
    Expression<Model> in(const Range& values) const {
        typename Expression<Model>::Node node;
        node.field = field_;
        node.comparison = CompareOp::In;
        for (const auto& value : values) node.values.push_back(bindValue(value));
        if (node.values.empty()) throw std::invalid_argument("IN requires at least one value");
        return Expression<Model>(std::move(node));
    }

    Expression<Model> isNull() const { return predicate(CompareOp::IsNull); }
    Expression<Model> isNotNull() const { return predicate(CompareOp::IsNotNull); }
    constexpr Order<Model> asc() const { return {field_, SortDirection::Asc}; }
    constexpr Order<Model> desc() const { return {field_, SortDirection::Desc}; }

private:
    template <typename V>
    Expression<Model> compare(CompareOp operation, V&& value) const {
        auto expression = predicate(operation);
        expression.node()->values.push_back(bindValue(std::forward<V>(value)));
        return expression;
    }

    Expression<Model> predicate(CompareOp operation) const {
        typename Expression<Model>::Node node;
        node.field = field_;
        node.comparison = operation;
        return Expression<Model>(std::move(node));
    }

    std::size_t field_;
};

template <typename Model>
struct QuerySpec {
    std::optional<Expression<Model>> where;
    std::vector<Order<Model>> orderBy;
    std::optional<std::size_t> limit;
    std::optional<std::size_t> offset;
    bool lockForUpdate = false;
};

template <typename Model, typename Repository>
class QueryBuilder {
public:
    explicit QueryBuilder(const Repository& repository) : repository_(&repository) {}

    QueryBuilder& where(Expression<Model> expression) { spec_.where = std::move(expression); return *this; }
    QueryBuilder& orderBy(Order<Model> order) { spec_.orderBy.push_back(order); return *this; }
    QueryBuilder& limit(std::size_t count) { spec_.limit = count; return *this; }
    QueryBuilder& offset(std::size_t count) { spec_.offset = count; return *this; }
    QueryBuilder& forUpdate(bool enabled = true) { spec_.lockForUpdate = enabled; return *this; }

    Task<std::vector<Model>> all() const { co_return co_await repository_->executeMany(spec_); }
    Task<std::optional<Model>> one() const { co_return co_await repository_->executeOne(spec_); }
    Task<std::size_t> deleteMany() const { co_return co_await repository_->executeDelete(spec_, false); }
    Task<std::size_t> deleteAll(decltype(nullptr)) const { co_return co_await repository_->executeDelete(spec_, true); }

private:
    const Repository* repository_;
    QuerySpec<Model> spec_;
};

inline constexpr std::nullptr_t confirmAllRows = nullptr;

} // namespace xp::data
