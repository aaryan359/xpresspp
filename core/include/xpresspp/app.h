#pragma once

#include "request.h"
#include "response.h"
#include "router.h"

#include <drogon/drogon.h>
#include <json/json.h>

#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace xp {

class HttpError : public std::runtime_error {
private:
    int status_code_;

public:
    HttpError(int status_code, const std::string& message)
        : std::runtime_error(message), status_code_(status_code) {}

    int statusCode() const {
        return status_code_;
    }
};

using ErrorHandler = std::function<void(const std::exception&, Request&, Response&)>;
using AfterHandler = std::function<void(Request&, Response&)>;
using RouterBuilder = std::function<void(Router&)>;

struct AppConfig {
    std::string host = "0.0.0.0";
    int port = 8080;
    bool debug = true;
    bool caseSensitiveRouting = true;
    bool strictTrailingSlash = true;
    bool gzip = false;
    bool sendfile = true;
    std::size_t maxBodySize = 1024 * 1024;
    std::size_t threads = 0;
};

class App {
private:
    Router router_;
    std::vector<Middleware> middleware_;
    std::vector<AfterHandler> after_handlers_;
    ErrorHandler error_handler_;
    Handler not_found_handler_;
    Handler method_not_allowed_handler_;
    AppConfig config_;
    bool debug_ = true;

    struct StaticMount {
        std::string prefix;
        std::filesystem::path directory;
        bool spa_fallback = false;
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
            const auto path = req.path();

            if (path.rfind(prefix, 0) != 0) {
                continue;
            }

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

            if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
                continue;
            }

            res.file(file_path);
            return true;
        }

        return false;
    }

    void runAfterHandlers(Request& req, Response& res) const {
        for (const auto& handler : after_handlers_) {
            handler(req, res);
        }
    }

    void handleError(const std::exception& error, Request& req, Response& res) const {
        if (error_handler_) {
            error_handler_(error, req, res);
            return;
        }

        int status_code = 500;
        if (const auto* http_error = dynamic_cast<const HttpError*>(&error)) {
            status_code = http_error->statusCode();
        } else if (dynamic_cast<const std::invalid_argument*>(&error)) {
            status_code = 400;
        }

        Json::Value payload;
        payload["status"] = "error";
        payload["message"] = error.what();
        payload["method"] = req.method();
        payload["path"] = req.path();
        if (debug_) {
            payload["hint"] = "Handle this with app.onError(...) for a custom response.";
        }

        res.status(status_code).json(payload);
    }

    static void runMiddlewareStack(const std::vector<Middleware>& stack,
                                   std::size_t index,
                                   Request& req,
                                   Response& res,
                                   const Handler& final_handler) {
        if (index >= stack.size()) {
            final_handler(req, res);
            return;
        }

        bool next_called = false;
        Next next = [&]() {
            next_called = true;
            runMiddlewareStack(stack, index + 1, req, res, final_handler);
        };

        stack[index](req, res, next);

        if (!next_called && !res.sent()) {
            throw HttpError(500, "Middleware ended without calling next() or sending a response.");
        }
    }

    void handleRequest(const drogon::HttpRequestPtr& native_req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        Request req(native_req);
        Response res;

        try {
            Handler final_handler = [&](Request& final_req, Response& final_res) {
                if (serveStatic(final_req, final_res)) {
                    return;
                }

                auto route = router_.match(final_req.method(), final_req.path());
                if (!route.matched) {
                    const auto allowed = router_.allowedMethods(final_req.path());
                    if (!allowed.empty()) {
                        if (method_not_allowed_handler_) {
                            method_not_allowed_handler_(final_req, final_res);
                            return;
                        }
                        final_res.header("Allow", [&]() {
                            std::string value;
                            for (std::size_t i = 0; i < allowed.size(); ++i) {
                                if (i > 0) value += ", ";
                                value += allowed[i];
                            }
                            return value;
                        }());
                        throw HttpError(405, "Method " + final_req.method() + " is not allowed for " + final_req.path());
                    }
                    if (not_found_handler_) {
                        not_found_handler_(final_req, final_res);
                        return;
                    }
                    throw HttpError(404, "Cannot " + final_req.method() + " " + final_req.path());
                }

                for (const auto& param : route.params) {
                    final_req.setParam(param.first, param.second);
                }

                if (route.middleware.empty()) {
                    route.handler(final_req, final_res);
                    return;
                }

                runMiddlewareStack(route.middleware, 0, final_req, final_res, route.handler);
            };

            runMiddlewareStack(middleware_, 0, req, res, final_handler);
        } catch (const std::exception& error) {
            handleError(error, req, res);
        } catch (...) {
            HttpError unknown(500, "Unknown C++ exception escaped the request handler.");
            handleError(unknown, req, res);
        }

        runAfterHandlers(req, res);
        callback(res.native());
    }

public:
    App() {
        notFound([](Request& req, Response& res) {
            res.status(404).json({
                {"status", "error"},
                {"message", "Cannot " + req.method() + " " + req.path()}
            });
        });
    }

    App& debug(bool enabled) {
        debug_ = enabled;
        config_.debug = enabled;
        return *this;
    }

    App& configure(const AppConfig& config) {
        config_ = config;
        debug_ = config.debug;
        router_.caseSensitive(config.caseSensitiveRouting);
        router_.strictTrailingSlash(config.strictTrailingSlash);
        return *this;
    }

    const AppConfig& config() const {
        return config_;
    }

    App& use(Middleware middleware) {
        middleware_.push_back(std::move(middleware));
        return *this;
    }

    App& use(const std::string& prefix, const Router& router) {
        auto mount_prefix = trimTrailingSlash(prefix);
        const auto mounted_router = router;
        middleware_.push_back([mount_prefix, mounted_router](Request& req, Response& res, Next next) mutable {
            if (req.path().rfind(mount_prefix, 0) != 0) {
                next();
                return;
            }

            auto relative_path = req.path().substr(mount_prefix.size());
            if (relative_path.empty()) {
                relative_path = "/";
            }

            auto route = mounted_router.match(req.method(), relative_path);
            if (!route.matched) {
                next();
                return;
            }
            for (const auto& param : route.params) {
                req.setParam(param.first, param.second);
            }
            route.handler(req, res);
        });
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

    App& staticFiles(const std::filesystem::path& directory,
                     const std::string& prefix = "/",
                     bool spa_fallback = false) {
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

    App& get(const std::string& path, Handler handler) { router_.get(path, std::move(handler)); return *this; }
    App& post(const std::string& path, Handler handler) { router_.post(path, std::move(handler)); return *this; }
    App& put(const std::string& path, Handler handler) { router_.put(path, std::move(handler)); return *this; }
    App& patch(const std::string& path, Handler handler) { router_.patch(path, std::move(handler)); return *this; }
    App& del(const std::string& path, Handler handler) { router_.del(path, std::move(handler)); return *this; }
    App& options(const std::string& path, Handler handler) { router_.options(path, std::move(handler)); return *this; }
    App& head(const std::string& path, Handler handler) { router_.head(path, std::move(handler)); return *this; }
    App& all(const std::string& path, Handler handler) { router_.all(path, std::move(handler)); return *this; }

    App& get(const std::string& path, std::vector<Middleware> middleware, Handler handler) { router_.get(path, std::move(middleware), std::move(handler)); return *this; }
    App& post(const std::string& path, std::vector<Middleware> middleware, Handler handler) { router_.post(path, std::move(middleware), std::move(handler)); return *this; }
    App& put(const std::string& path, std::vector<Middleware> middleware, Handler handler) { router_.put(path, std::move(middleware), std::move(handler)); return *this; }
    App& patch(const std::string& path, std::vector<Middleware> middleware, Handler handler) { router_.patch(path, std::move(middleware), std::move(handler)); return *this; }
    App& del(const std::string& path, std::vector<Middleware> middleware, Handler handler) { router_.del(path, std::move(middleware), std::move(handler)); return *this; }
    App& options(const std::string& path, std::vector<Middleware> middleware, Handler handler) { router_.options(path, std::move(middleware), std::move(handler)); return *this; }
    App& head(const std::string& path, std::vector<Middleware> middleware, Handler handler) { router_.head(path, std::move(middleware), std::move(handler)); return *this; }
    App& all(const std::string& path, std::vector<Middleware> middleware, Handler handler) { router_.all(path, std::move(middleware), std::move(handler)); return *this; }

    std::vector<RouteInfo> routes() const {
        return router_.routes();
    }

    void listen(int port) {
        listen("0.0.0.0", port);
    }

    void listen(const std::string& host, int port) {
        auto& framework = drogon::app();
        framework.enableGzip(config_.gzip)
                 .enableSendfile(config_.sendfile)
                 .setClientMaxBodySize(config_.maxBodySize);
        if (config_.threads > 0) {
            framework.setThreadNum(config_.threads);
        }

        framework.registerHandlerViaRegex(
            "(.*)",
            [this](const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                handleRequest(req, std::move(callback));
            }
        );

        framework.addListener(host, port).run();
    }

    void listen() {
        listen(config_.host, config_.port);
    }

    void listenTls(const std::string& host,
                   int port,
                   const std::string& cert_file,
                   const std::string& key_file) {
        auto& framework = drogon::app();
        framework.enableGzip(config_.gzip)
                 .enableSendfile(config_.sendfile)
                 .setClientMaxBodySize(config_.maxBodySize);
        if (config_.threads > 0) {
            framework.setThreadNum(config_.threads);
        }
        framework.registerHandlerViaRegex(
            "(.*)",
            [this](const drogon::HttpRequestPtr& req,
                   std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
                handleRequest(req, std::move(callback));
            }
        );

        framework.addListener(host, static_cast<uint16_t>(port), true, cert_file, key_file).run();
    }

    static void shutdown() {
        drogon::app().quit();
    }
};

} // namespace xp
