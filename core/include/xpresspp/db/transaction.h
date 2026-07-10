#pragma once

#include "../database.h"

#include <coroutine>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace xp::data {

class TransactionContext {
public:
    explicit TransactionContext(drogon::orm::TransactionPtr transaction)
        : transaction_(std::move(transaction)) {
        if (!transaction_) throw std::runtime_error("Failed to start database transaction");
    }
    TransactionContext(const TransactionContext&) = delete;
    TransactionContext& operator=(const TransactionContext&) = delete;
    TransactionContext(TransactionContext&&) = delete;
    TransactionContext& operator=(TransactionContext&&) = delete;

    drogon::orm::DbClientPtr client() const { return transaction_; }
    bool active() const { return static_cast<bool>(transaction_); }

    class CommitAwaiter {
    public:
        explicit CommitAwaiter(drogon::orm::TransactionPtr transaction)
            : transaction_(std::move(transaction)) {}
        bool await_ready() const noexcept { return false; }
        void await_suspend(std::coroutine_handle<> handle) {
            transaction_->setCommitCallback([this, handle](bool success) mutable {
                success_ = success;
                handle.resume();
            });
            transaction_.reset(); // Drogon commits when the final transaction owner is released.
        }
        void await_resume() const {
            if (!success_) throw std::runtime_error("Database transaction commit failed");
        }
    private:
        drogon::orm::TransactionPtr transaction_;
        bool success_ = false;
    };

    CommitAwaiter commit() {
        if (!transaction_) throw std::logic_error("Transaction is already closed");
        return CommitAwaiter(std::move(transaction_));
    }

    void rollback() {
        if (!transaction_) return;
        transaction_->rollback();
        transaction_.reset();
    }

    ~TransactionContext() {
        if (transaction_) rollback();
    }

private:
    drogon::orm::TransactionPtr transaction_;
};

template <typename T> struct TaskValue;
template <typename T> struct TaskValue<drogon::Task<T>> { using type = T; };
template <typename T> using TaskValueT = typename TaskValue<std::remove_cvref_t<T>>::type;

template <typename Client, typename Factory, typename Work>
auto runTransaction(drogon::orm::DbClientPtr database, Factory&& factory, Work&& work)
    -> drogon::Task<TaskValueT<std::invoke_result_t<Work, Client&>>> {
    using Result = TaskValueT<std::invoke_result_t<Work, Client&>>;
    auto transaction = co_await database->newTransactionCoro();
    TransactionContext context(std::move(transaction));
    Client client = std::forward<Factory>(factory)(context);
    try {
        if constexpr (std::is_void_v<Result>) {
            co_await std::forward<Work>(work)(client);
            co_await context.commit();
            co_return;
        } else {
            Result result = co_await std::forward<Work>(work)(client);
            co_await context.commit();
            co_return result;
        }
    } catch (...) {
        context.rollback();
        throw;
    }
}

} // namespace xp::data
