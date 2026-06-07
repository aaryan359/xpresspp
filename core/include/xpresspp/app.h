#pragma once

#include "errors.h"
#include "request.h"
#include "response.h"
#include "router.h"
#include "database.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <cstdio>
#include <exception>
#include <filesystem>
#include <functional>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <variant>
#include <type_traits>
#include <future>
#include <mutex>
#include <condition_variable>

#if defined(_WIN32)
#include <io.h>
#define isatty _isatty
#define fileno _fileno
#else
#include <unistd.h>
#endif

namespace xp {

class App;

// ────────────────────────────────────────────────────────────
//  RequestContext
// ────────────────────────────────────────────────────────────
using ErrorHandler = std::function<void(const std::exception&, Request&, Response&)>;
using AfterHandler = std::function<void(Request&, Response&)>;
using RouterBuilder = std::function<void(Router&)>;

struct RequestContext {
    Request req;
    Response res;
    std::vector<AfterHandler> after_handlers;
    std::function<void(const drogon::HttpResponsePtr&)> callback;
    bool finished = false;

    RequestContext(const drogon::HttpRequestPtr& native_req,
                   std::vector<AfterHandler> handlers,
                   std::function<void(const drogon::HttpResponsePtr&)> cb)
        : req(native_req), after_handlers(std::move(handlers)), callback(std::move(cb)) {}

    void finish() {
        if (finished) return;
        finished = true;
        for (const auto& handler : after_handlers) {
            handler(req, res);
        }
        callback(res.native());
    }
};

// Thread-local request context pointer to resolve async sub-routers
inline thread_local std::shared_ptr<RequestContext> current_request_context = nullptr;

// ────────────────────────────────────────────────────────────
//  AppConfig
// ────────────────────────────────────────────────────────────
struct AppConfig {
    std::string host              = "0.0.0.0";
    int         port              = 8080;
    bool        debug             = true;
    bool        caseSensitiveRouting = true;
    bool        strictTrailingSlash  = true;
    bool        gzip              = false;
    bool        sendfile          = true;
    bool        showBanner        = true;   // print startup banner
    std::size_t maxBodySize       = 1024 * 1024;
    std::size_t threads           = 0;
};

// ────────────────────────────────────────────────────────────
//  Internal ANSI helpers
// ────────────────────────────────────────────────────────────
namespace detail {
inline bool appColorOk() {
#if defined(_WIN32)
    return false;
#else
    return isatty(fileno(stderr)) != 0;
#endif
}
inline const char* appRed()   { return appColorOk() ? "\033[31m" : ""; }
inline const char* appYellow(){ return appColorOk() ? "\033[33m" : ""; }
inline const char* appGreen() { return appColorOk() ? "\033[32m" : ""; }
inline const char* appCyan()  { return appColorOk() ? "\033[36m" : ""; }
inline const char* appBold()  { return appColorOk() ? "\033[1m"  : ""; }
inline const char* appDim()   { return appColorOk() ? "\033[2m"  : ""; }
inline const char* appReset() { return appColorOk() ? "\033[0m"  : ""; }

inline void printBanner(const std::string& host, int port, bool debug) {
    const char* env = debug ? "development" : "production";
    std::cout
        << "\n"
        << appCyan() << appBold()
        << "  ╔═══════════════════════════════════╗\n"
        << "  ║        xpress++  is ready         ║\n"
        << "  ╚═══════════════════════════════════╝"
        << appReset() << "\n\n"
        << "  " << appBold() << "Listening on" << appReset()
        << "  http://" << host << ":" << port << "\n"
        << "  " << appBold() << "Environment" << appReset()
        << "  " << (debug ? appYellow() : appGreen())
        << env << appReset() << "\n"
        << "\n"
        << appDim() << "  Press Ctrl+C to stop.\n" << appReset()
        << "\n";
}

inline void printStartupError(const std::string& heading,
                              const std::string& detail,
                              const std::string& fix = "") {
    std::cerr
        << "\n"
        << appRed() << appBold()
        << "  [xpress++ startup error]\n"
        << appReset()
        << appRed()
        << "  " << heading << "\n"
        << appReset();
    if (!detail.empty()) {
        std::cerr << appDim() << "  " << detail << "\n" << appReset();
    }
    if (!fix.empty()) {
        std::cerr
            << "\n"
            << appYellow() << appBold() << "  Fix: " << appReset()
            << appYellow() << fix << "\n" << appReset();
    }
    std::cerr << "\n";
}
} // namespace detail

// ────────────────────────────────────────────────────────────
//  App
// ────────────────────────────────────────────────────────────
class App {
private:
    Router                    router_;
    std::vector<Middleware>   middleware_;
    std::vector<AfterHandler> after_handlers_;
    ErrorHandler              error_handler_;
    Handler                   not_found_handler_;
    Handler                   method_not_allowed_handler_;
    AppConfig                 config_;
    bool                      debug_ = true;

    struct StaticMount {
        std::string          prefix;
        std::filesystem::path directory;
        bool                  spa_fallback = false;
    };
    std::vector<StaticMount> static_mounts_;

    static std::string trimTrailingSlash(std::string value) {
        while (value.size() > 1 && value.back() == '/') {
            value.pop_back();
        }
        return value;
    }

    bool serveStatic(Request& req, Response& res) const {
        for (const auto& mount : static_mounts_) {
            const auto prefix = trimTrailingSlash(mount.prefix);
            const auto path   = req.path();

            if (path.rfind(prefix, 0) != 0) continue;

            std::string relative = path.substr(prefix.size());
            if (relative.empty() || relative == "/") {
                relative = "/index.html";
            }

            std::filesystem::path file_path = mount.directory / relative.substr(1);
            if (std::filesystem::is_directory(file_path)) {
                file_path /= "index.html";
            }

            if (!std::filesystem::exists(file_path) && mount.spa_fallback) {
                file_path = mount.directory / "index.html";
            }

            if (!std::filesystem::exists(file_path) ||
                !std::filesystem::is_regular_file(file_path)) {
                continue;
            }

            res.file(file_path);
            return true;
        }
        return false;
    }

public:
    void handleError(const std::exception& error, Request& req, Response& res) const {
        if (error_handler_) {
            try {
                error_handler_(error, req, res);
            } catch (const std::exception& inner) {
                res.status(500);
                Json::Value fallback;
                fallback["status"]  = "error";
                fallback["message"] = "Error handler threw an exception: " +
                                      std::string(inner.what());
                res.json(fallback);
            }
            return;
        }

        int         status_code = 500;
        std::string hint;

        if (const auto* he = dynamic_cast<const HttpError*>(&error)) {
            status_code = he->statusCode();
            hint        = he->hint();
        } else if (dynamic_cast<const std::invalid_argument*>(&error)) {
            status_code = 400;
            hint        = "Check the request body or parameters for malformed data.";
        } else if (dynamic_cast<const std::out_of_range*>(&error)) {
            status_code = 400;
            hint        = "A value is outside of the expected range.";
        } else if (dynamic_cast<const std::logic_error*>(&error)) {
            status_code = 500;
            hint        = "A logic error occurred. This is likely a bug in the application.";
        }

        Json::Value payload;
        payload["status"]  = "error";
        payload["message"] = error.what();
        payload["path"]    = req.path();
        payload["method"]  = req.method();

        if (debug_) {
            if (!hint.empty()) {
                payload["hint"] = hint;
            } else if (status_code >= 500) {
                payload["hint"] =
                    "An unhandled exception escaped your route handler. "
                    "Wrap your logic in try/catch, or add app.onError(...) "
                    "to handle all errors globally.";
            }
        }

        if (debug_ && status_code >= 500) {
            std::cerr
                << detail::appRed() << detail::appBold()
                << "\n  [xpress++ error] "
                << detail::appReset()
                << detail::appRed()
                << req.method() << " " << req.path()
                << detail::appReset()
                << "\n  " << error.what() << "\n";
            if (!hint.empty()) {
                std::cerr
                    << detail::appYellow()
                    << "  Hint: " << hint
                    << detail::appReset() << "\n";
            }
            std::cerr << "\n";
        }

        res.status(status_code).json(payload);
    }

private:
    void runMiddlewareStack(const std::vector<Middleware>& stack,
                            std::size_t                    index,
                            std::shared_ptr<RequestContext> ctx,
                            const std::function<void(std::shared_ptr<RequestContext>&)>& final_handler) {
        if (index >= stack.size()) {
            final_handler(ctx);
            return;
        }

        bool next_called = false;
        Next next = [this, stack, index, ctx, final_handler, &next_called]() {
            next_called = true;
            runMiddlewareStack(stack, index + 1, ctx, final_handler);
        };

        try {
            stack[index](ctx->req, ctx->res, next);
        } catch (const std::exception& error) {
            handleError(error, ctx->req, ctx->res);
            ctx->finish();
            return;
        } catch (...) {
            InternalError unknown(
                "An unknown C++ exception escaped a middleware.",
                "Ensure all thrown types derive from std::exception."
            );
            handleError(unknown, ctx->req, ctx->res);
            ctx->finish();
            return;
        }

        if (!next_called) {
            if (!ctx->res.sent()) {
                InternalError wrapped(
                    "Middleware at index " + std::to_string(index) +
                    " finished without calling next() or sending a response.",
                    "Every middleware must either call next() or send a response."
                );
                handleError(wrapped, ctx->req, ctx->res);
            }
            ctx->finish();
        }
    }

public:
    void handleRequest(const drogon::HttpRequestPtr&                       native_req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        auto ctx = std::make_shared<RequestContext>(native_req, after_handlers_, std::move(callback));
        current_request_context = ctx;

        try {
            auto final_handler = [this](std::shared_ptr<RequestContext>& f_ctx) {
                if (serveStatic(f_ctx->req, f_ctx->res)) {
                    f_ctx->finish();
                    return;
                }

                auto route = router_.match(f_ctx->req.method(), f_ctx->req.path());
                if (!route.matched) {
                    const auto allowed = router_.allowedMethods(f_ctx->req.path());
                    if (!allowed.empty()) {
                        if (std::holds_alternative<SyncHandler>(method_not_allowed_handler_)) {
                            std::get<SyncHandler>(method_not_allowed_handler_)(f_ctx->req, f_ctx->res);
                            f_ctx->finish();
                            return;
                        }
                        std::string allow_value;
                        for (std::size_t i = 0; i < allowed.size(); ++i) {
                            if (i > 0) allow_value += ", ";
                            allow_value += allowed[i];
                        }
                        f_ctx->res.header("Allow", allow_value);

                        MethodNotAllowedError err(f_ctx->req.method(), f_ctx->req.path());
                        handleError(err, f_ctx->req, f_ctx->res);
                        f_ctx->finish();
                        return;
                    }

                    if (std::holds_alternative<SyncHandler>(not_found_handler_)) {
                        std::get<SyncHandler>(not_found_handler_)(f_ctx->req, f_ctx->res);
                        f_ctx->finish();
                        return;
                    }
                    NotFoundError err(
                        "Cannot " + f_ctx->req.method() + " " + f_ctx->req.path(),
                        "Make sure you've registered a route for this path."
                    );
                    handleError(err, f_ctx->req, f_ctx->res);
                    f_ctx->finish();
                    return;
                }

                for (const auto& param : route.params) {
                    f_ctx->req.setParam(param.first, param.second);
                }

                auto run_route_handler = [this](std::shared_ptr<RequestContext> r_ctx, Handler handler) {
                    if (std::holds_alternative<SyncHandler>(handler)) {
                        try {
                            std::get<SyncHandler>(handler)(r_ctx->req, r_ctx->res);
                        } catch (const std::exception& error) {
                            handleError(error, r_ctx->req, r_ctx->res);
                        } catch (...) {
                            InternalError unknown("An unknown exception escaped.");
                            handleError(unknown, r_ctx->req, r_ctx->res);
                        }
                        r_ctx->finish();
                    } else {
                        auto coro_h = std::get<CoroHandler>(handler);
                        drogon::app().getLoop()->queueInLoop(
                            drogon::async_func([this, r_ctx, coro_h]() -> drogon::Task<void> {
                                try {
                                    co_await coro_h(r_ctx->req, r_ctx->res);
                                } catch (const std::exception& error) {
                                    handleError(error, r_ctx->req, r_ctx->res);
                                } catch (...) {
                                    InternalError unknown("An unknown exception escaped.");
                                    handleError(unknown, r_ctx->req, r_ctx->res);
                                }
                                r_ctx->finish();
                                co_return;
                            })
                        );
                    }
                };

                if (route.middleware.empty()) {
                    run_route_handler(f_ctx, route.handler);
                    return;
                }

                auto final_route_call = [run_route_handler, route](std::shared_ptr<RequestContext>& r_ctx) {
                    run_route_handler(r_ctx, route.handler);
                };

                runMiddlewareStack(route.middleware, 0, f_ctx, final_route_call);
            };

            runMiddlewareStack(middleware_, 0, ctx, final_handler);

        } catch (const std::exception& error) {
            handleError(error, ctx->req, ctx->res);
            ctx->finish();
        } catch (...) {
            InternalError unknown("An unknown exception escaped the pipeline.");
            handleError(unknown, ctx->req, ctx->res);
            ctx->finish();
        }

        current_request_context = nullptr;
    }

    // Public test injection endpoint for Supertest-style in-memory testing
    void injectRequest(const drogon::HttpRequestPtr& req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        handleRequest(req, std::move(callback));
    }

    void validateConfig(const std::string& host, int port) const {
        if (port <= 0 || port > 65535) {
            detail::printStartupError(
                "Invalid port: " + std::to_string(port),
                "Ports must be in the range 1–65535.",
                "Use a valid port, e.g. app.listen(8080)."
            );
            throw ConfigurationError(
                "Invalid port: " + std::to_string(port),
                "Ports must be in the range 1–65535."
            );
        }
        if (port < 1024 && port != 80 && port != 443) {
            std::cerr
                << detail::appYellow()
                << "  [xpress++ warning] Port " << port
                << " is a privileged port (< 1024). "
                   "You may need root/sudo to bind to it.\n"
                << detail::appReset();
        }
        if (host.empty()) {
            detail::printStartupError(
                "Host cannot be empty.",
                "",
                "Use \"0.0.0.0\" to listen on all interfaces, or \"127.0.0.1\" for localhost."
            );
            throw ConfigurationError("Host cannot be empty.");
        }
    }

    void applyCoreConfig(drogon::HttpAppFramework& fw) const {
        fw.enableGzip(config_.gzip)
          .enableSendfile(config_.sendfile)
          .setClientMaxBodySize(config_.maxBodySize);
        if (config_.threads > 0) {
            fw.setThreadNum(config_.threads);
        }
        fw.registerHandlerViaRegex(
            "(.*)",
            [this](const drogon::HttpRequestPtr&                       r,
                   std::function<void(const drogon::HttpResponsePtr&)>&& cb) {
                const_cast<App*>(this)->handleRequest(r, std::move(cb));
            }
        );
    }

    App() {
        notFound([](Request& req, Response& res) {
            res.status(404).json({
                {"status",  "error"},
                {"message", "Cannot " + req.method() + " " + req.path()}
            });
        });
    }

    App& debug(bool enabled) {
        debug_         = enabled;
        config_.debug  = enabled;
        return *this;
    }

    App& configure(const AppConfig& config) {
        config_ = config;
        debug_  = config.debug;
        router_.caseSensitive(config.caseSensitiveRouting);
        router_.strictTrailingSlash(config.strictTrailingSlash);
        return *this;
    }

    const AppConfig& config() const { return config_; }

    App& database(const DbConfig& db_config) {
        std::string driver = db_config.driver;
        if (driver == "sqlite" || driver == "sqlite3") {
            driver = "sqlite3";
        } else if (driver == "postgres" || driver == "postgresql") {
            driver = "postgresql";
        } else if (driver == "mysql") {
            driver = "mysql";
        }

        try {
            if (driver == "sqlite3") {
                drogon::orm::Sqlite3Config config{};
                config.connectionNumber = db_config.connection_number;
                config.filename = db_config.database;
                config.name = db_config.name;
                config.timeout = 0.0;
                drogon::app().addDbClient(config);
            } else if (driver == "postgresql") {
                drogon::orm::PostgresConfig config{};
                config.host = db_config.host;
                config.port = static_cast<unsigned short>(db_config.port);
                config.databaseName = db_config.database;
                config.username = db_config.username;
                config.password = db_config.password;
                config.connectionNumber = db_config.connection_number;
                config.name = db_config.name;
                config.isFast = false;
                drogon::app().addDbClient(config);
            } else if (driver == "mysql") {
                drogon::orm::MysqlConfig config{};
                config.host = db_config.host;
                config.port = static_cast<unsigned short>(db_config.port);
                config.databaseName = db_config.database;
                config.username = db_config.username;
                config.password = db_config.password;
                config.connectionNumber = db_config.connection_number;
                config.name = db_config.name;
                config.isFast = false;
                drogon::app().addDbClient(config);
            } else {
                throw std::invalid_argument("Unsupported database driver: " + driver);
            }
        } catch (const std::exception& e) {
            std::cerr << "[xpress++ db error] Failed to create database client: " << e.what() << "\n";
            throw;
        }
        return *this;
    }

    App& use(Middleware middleware) {
        middleware_.push_back(std::move(middleware));
        return *this;
    }

    App& use(const std::string& prefix, const Router& router) {
        auto mount_prefix   = trimTrailingSlash(prefix);
        const auto mounted  = router;
        middleware_.push_back(
            [this, mount_prefix, mounted](Request& req, Response& res, Next next) mutable {
                if (req.path().rfind(mount_prefix, 0) != 0) { next(); return; }

                auto relative = req.path().substr(mount_prefix.size());
                if (relative.empty()) relative = "/";

                auto route = mounted.match(req.method(), relative);
                if (!route.matched) { next(); return; }

                for (const auto& p : route.params) {
                    req.setParam(p.first, p.second);
                }

                if (std::holds_alternative<SyncHandler>(route.handler)) {
                    std::get<SyncHandler>(route.handler)(req, res);
                } else {
                    auto* ctx = current_request_context.get();
                    if (ctx) {
                        auto coro_h = std::get<CoroHandler>(route.handler);
                        auto launch = [this, ctx, coro_h]() -> drogon::AsyncTask {
                            try {
                                co_await coro_h(ctx->req, ctx->res);
                            } catch (const std::exception& e) {
                                const_cast<App*>(this)->handleError(e, ctx->req, ctx->res);
                            } catch (...) {
                                InternalError unknown("An unknown exception escaped.");
                                const_cast<App*>(this)->handleError(unknown, ctx->req, ctx->res);
                            }
                            ctx->finish();
                            co_return;
                        };
                        launch();
                    }
                }
            }
        );
        return *this;
    }

    App& group(const std::string& prefix, RouterBuilder builder) {
        Router grouped;
        grouped.caseSensitive(config_.caseSensitiveRouting);
        grouped.strictTrailingSlash(config_.strictTrailingSlash);
        builder(grouped);
        router_.use(prefix, grouped);
        return *this;
    }

    App& after(AfterHandler handler) {
        after_handlers_.push_back(std::move(handler));
        return *this;
    }

    App& onStart(std::function<Task()> callback) {
        drogon::app().registerBeginningAdvice([callback = std::move(callback)]() {
            auto run_callback = [callback]() -> drogon::AsyncTask {
                try {
                    co_await callback();
                } catch (const std::exception& e) {
                    std::cerr << "[xpress++ startup error] " << e.what() << std::endl;
                }
                co_return;
            };
            run_callback();
        });
        return *this;
    }

    App& staticFiles(const std::filesystem::path& directory,
                     const std::string&           prefix       = "/",
                     bool                         spa_fallback = false) {
        if (!std::filesystem::exists(directory)) {
            std::cerr
                << detail::appYellow()
                << "  [xpress++ warning] staticFiles() path does not exist: "
                << directory << "\n"
                << detail::appReset();
        }
        static_mounts_.push_back(StaticMount{prefix, directory, spa_fallback});
        return *this;
    }

    App& onError(ErrorHandler handler) {
        error_handler_ = std::move(handler);
        return *this;
    }

    App& notFound(Handler handler) {
        not_found_handler_ = std::move(handler);
        return *this;
    }

    App& methodNotAllowed(Handler handler) {
        method_not_allowed_handler_ = std::move(handler);
        return *this;
    }

    template <typename H> App& get    (const std::string& path, H&& h) { router_.get    (path, std::forward<H>(h)); return *this; }
    template <typename H> App& post   (const std::string& path, H&& h) { router_.post   (path, std::forward<H>(h)); return *this; }
    template <typename H> App& put    (const std::string& path, H&& h) { router_.put    (path, std::forward<H>(h)); return *this; }
    template <typename H> App& patch  (const std::string& path, H&& h) { router_.patch  (path, std::forward<H>(h)); return *this; }
    template <typename H> App& del    (const std::string& path, H&& h) { router_.del    (path, std::forward<H>(h)); return *this; }
    template <typename H> App& options(const std::string& path, H&& h) { router_.options(path, std::forward<H>(h)); return *this; }
    template <typename H> App& head   (const std::string& path, H&& h) { router_.head   (path, std::forward<H>(h)); return *this; }
    template <typename H> App& all    (const std::string& path, H&& h) { router_.all    (path, std::forward<H>(h)); return *this; }

    template <typename H> App& get    (const std::string& path, Middleware mw, H&& h) { router_.get    (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& post   (const std::string& path, Middleware mw, H&& h) { router_.post   (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& put    (const std::string& path, Middleware mw, H&& h) { router_.put    (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& patch  (const std::string& path, Middleware mw, H&& h) { router_.patch  (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& del    (const std::string& path, Middleware mw, H&& h) { router_.del    (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& options(const std::string& path, Middleware mw, H&& h) { router_.options(path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& head   (const std::string& path, Middleware mw, H&& h) { router_.head   (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& all    (const std::string& path, Middleware mw, H&& h) { router_.all    (path, std::move(mw), std::forward<H>(h)); return *this; }

    template <typename H> App& get    (const std::string& path, std::vector<Middleware> mw, H&& h) { router_.get    (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& post   (const std::string& path, std::vector<Middleware> mw, H&& h) { router_.post   (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& put    (const std::string& path, std::vector<Middleware> mw, H&& h) { router_.put    (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& patch  (const std::string& path, std::vector<Middleware> mw, H&& h) { router_.patch  (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& del    (const std::string& path, std::vector<Middleware> mw, H&& h) { router_.del    (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& options(const std::string& path, std::vector<Middleware> mw, H&& h) { router_.options(path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& head   (const std::string& path, std::vector<Middleware> mw, H&& h) { router_.head   (path, std::move(mw), std::forward<H>(h)); return *this; }
    template <typename H> App& all    (const std::string& path, std::vector<Middleware> mw, H&& h) { router_.all    (path, std::move(mw), std::forward<H>(h)); return *this; }

    std::vector<RouteInfo> routes() const { return router_.routes(); }

    void listen(int port) {
        listen("0.0.0.0", port);
    }

    void listen(const std::string& host, int port) {
        validateConfig(host, port);

        auto& fw = drogon::app();
        applyCoreConfig(fw);

        if (config_.showBanner) {
            detail::printBanner(host == "0.0.0.0" ? "localhost" : host, port, debug_);
        }

        try {
            fw.addListener(host, port).run();
        } catch (const std::exception& e) {
            detail::printStartupError(
                "Failed to start server on " + host + ":" + std::to_string(port),
                e.what(),
                "Is another process already using port " + std::to_string(port) + "? "
                "Try: lsof -i :" + std::to_string(port)
            );
            throw;
        }
    }

    void listen() {
        listen(config_.host, config_.port);
    }

    void listenTls(const std::string& host,
                   int                port,
                   const std::string& cert_file,
                   const std::string& key_file) {
        validateConfig(host, port);

        if (!std::filesystem::exists(cert_file)) {
            detail::printStartupError(
                "TLS certificate file not found: " + cert_file,
                "",
                "Generate a self-signed cert with:\n"
                "    openssl req -x509 -newkey rsa:4096 -keyout key.pem "
                "-out cert.pem -days 365 -nodes"
            );
            throw ConfigurationError("TLS certificate file not found: " + cert_file);
        }
        if (!std::filesystem::exists(key_file)) {
            detail::printStartupError(
                "TLS key file not found: " + key_file,
                "",
                "Ensure your private key exists and is readable."
            );
            throw ConfigurationError("TLS key file not found: " + key_file);
        }

        auto& fw = drogon::app();
        applyCoreConfig(fw);

        if (config_.showBanner) {
            detail::printBanner("https://" + (host == "0.0.0.0" ? "localhost" : host),
                                port, debug_);
        }

        try {
            fw.addListener(host, static_cast<uint16_t>(port), true, cert_file, key_file).run();
        } catch (const std::exception& e) {
            detail::printStartupError(
                "Failed to start TLS server on " + host + ":" + std::to_string(port),
                e.what(),
                "Check that your certificate and key files are valid and not expired."
            );
            throw;
        }
    }

    static void shutdown() {
        drogon::app().quit();
    }
};

} // namespace xp
