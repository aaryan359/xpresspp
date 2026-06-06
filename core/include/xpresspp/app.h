#pragma once

#include "errors.h"
#include "request.h"
#include "response.h"
#include "router.h"

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

namespace xp {

using ErrorHandler = std::function<void(const std::exception&, Request&, Response&)>;
using AfterHandler = std::function<void(Request&, Response&)>;
using RouterBuilder = std::function<void(Router&)>;

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
//  Internal ANSI helpers (duplicated minimally for app.h
//  independence from logger.h)
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

    // ──────────────────────────────────────────────────────
    //  Helpers
    // ──────────────────────────────────────────────────────
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

    void runAfterHandlers(Request& req, Response& res) const {
        for (const auto& handler : after_handlers_) {
            handler(req, res);
        }
    }

    // ──────────────────────────────────────────────────────
    //  Error handling — converts any exception into a clean
    //  JSON response with developer-friendly hints in debug.
    // ──────────────────────────────────────────────────────
    void handleError(const std::exception& error, Request& req, Response& res) const {
        // Delegate to user-supplied handler first
        if (error_handler_) {
            try {
                error_handler_(error, req, res);
            } catch (const std::exception& inner) {
                // The error handler itself threw — prevent infinite recursion
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

        // Also emit to stderr in debug so the dev sees it in the terminal
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

    // ──────────────────────────────────────────────────────
    //  Middleware stack runner
    // ──────────────────────────────────────────────────────
    static void runMiddlewareStack(const std::vector<Middleware>& stack,
                                   std::size_t                    index,
                                   Request&                       req,
                                   Response&                      res,
                                   const Handler&                 final_handler) {
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
            throw MiddlewareError(
                "Middleware at index " + std::to_string(index) +
                " finished without calling next() or sending a response.",
                "Every middleware must either call next() to pass control "
                "to the next middleware, or send a response "
                "(res.json(...), res.send(...), etc.)."
            );
        }
    }

    // ──────────────────────────────────────────────────────
    //  Per-request dispatch
    // ──────────────────────────────────────────────────────
    void handleRequest(const drogon::HttpRequestPtr&                       native_req,
                       std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        Request  req(native_req);
        Response res;

        try {
            Handler final_handler = [&](Request& final_req, Response& final_res) {
                // Static files first
                if (serveStatic(final_req, final_res)) return;

                auto route = router_.match(final_req.method(), final_req.path());
                if (!route.matched) {
                    const auto allowed = router_.allowedMethods(final_req.path());
                    if (!allowed.empty()) {
                        if (method_not_allowed_handler_) {
                            method_not_allowed_handler_(final_req, final_res);
                            return;
                        }
                        // Build Allow header
                        std::string allow_value;
                        for (std::size_t i = 0; i < allowed.size(); ++i) {
                            if (i > 0) allow_value += ", ";
                            allow_value += allowed[i];
                        }
                        final_res.header("Allow", allow_value);

                        throw MethodNotAllowedError(final_req.method(), final_req.path());
                    }

                    if (not_found_handler_) {
                        not_found_handler_(final_req, final_res);
                        return;
                    }
                    throw NotFoundError(
                        "Cannot " + final_req.method() + " " + final_req.path(),
                        "Make sure you've registered a route for this path, e.g. "
                        "app.get(\"" + final_req.path() + "\", ...)."
                    );
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

        } catch (const MiddlewareError& me) {
            // Promote MiddlewareError to a 500 with a dev hint
            InternalError wrapped(
                "Middleware configuration error: " + std::string(me.what()),
                me.fix()
            );
            handleError(wrapped, req, res);
        } catch (const std::exception& error) {
            handleError(error, req, res);
        } catch (...) {
            InternalError unknown(
                "An unknown C++ exception escaped the request handler.",
                "Ensure all thrown types derive from std::exception. "
                "Throwing raw ints, strings, or non-standard types is not supported."
            );
            handleError(unknown, req, res);
        }

        runAfterHandlers(req, res);
        callback(res.native());
    }

    // ──────────────────────────────────────────────────────
    //  Startup validation
    // ──────────────────────────────────────────────────────
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

public:
    // ──────────────────────────────────────────────────────
    //  Constructor: set sensible default not-found handler
    // ──────────────────────────────────────────────────────
    App() {
        notFound([](Request& req, Response& res) {
            res.status(404).json({
                {"status",  "error"},
                {"message", "Cannot " + req.method() + " " + req.path()}
            });
        });
    }

    // ──────────────────────────────────────────────────────
    //  Configuration
    // ──────────────────────────────────────────────────────
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

    // ──────────────────────────────────────────────────────
    //  Middleware
    // ──────────────────────────────────────────────────────
    App& use(Middleware middleware) {
        middleware_.push_back(std::move(middleware));
        return *this;
    }

    App& use(const std::string& prefix, const Router& router) {
        auto mount_prefix   = trimTrailingSlash(prefix);
        const auto mounted  = router;
        middleware_.push_back(
            [mount_prefix, mounted](Request& req, Response& res, Next next) mutable {
                if (req.path().rfind(mount_prefix, 0) != 0) { next(); return; }

                auto relative = req.path().substr(mount_prefix.size());
                if (relative.empty()) relative = "/";

                auto route = mounted.match(req.method(), relative);
                if (!route.matched) { next(); return; }

                for (const auto& p : route.params) {
                    req.setParam(p.first, p.second);
                }
                route.handler(req, res);
            }
        );
        return *this;
    }

    // ──────────────────────────────────────────────────────
    //  Routing
    // ──────────────────────────────────────────────────────
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

    // ──────────────────────────────────────────────────────
    //  Route registration
    // ──────────────────────────────────────────────────────
    App& get    (const std::string& path, Handler h) { router_.get    (path, std::move(h)); return *this; }
    App& post   (const std::string& path, Handler h) { router_.post   (path, std::move(h)); return *this; }
    App& put    (const std::string& path, Handler h) { router_.put    (path, std::move(h)); return *this; }
    App& patch  (const std::string& path, Handler h) { router_.patch  (path, std::move(h)); return *this; }
    App& del    (const std::string& path, Handler h) { router_.del    (path, std::move(h)); return *this; }
    App& options(const std::string& path, Handler h) { router_.options(path, std::move(h)); return *this; }
    App& head   (const std::string& path, Handler h) { router_.head   (path, std::move(h)); return *this; }
    App& all    (const std::string& path, Handler h) { router_.all    (path, std::move(h)); return *this; }

    App& get    (const std::string& path, std::vector<Middleware> mw, Handler h) { router_.get    (path, std::move(mw), std::move(h)); return *this; }
    App& post   (const std::string& path, std::vector<Middleware> mw, Handler h) { router_.post   (path, std::move(mw), std::move(h)); return *this; }
    App& put    (const std::string& path, std::vector<Middleware> mw, Handler h) { router_.put    (path, std::move(mw), std::move(h)); return *this; }
    App& patch  (const std::string& path, std::vector<Middleware> mw, Handler h) { router_.patch  (path, std::move(mw), std::move(h)); return *this; }
    App& del    (const std::string& path, std::vector<Middleware> mw, Handler h) { router_.del    (path, std::move(mw), std::move(h)); return *this; }
    App& options(const std::string& path, std::vector<Middleware> mw, Handler h) { router_.options(path, std::move(mw), std::move(h)); return *this; }
    App& head   (const std::string& path, std::vector<Middleware> mw, Handler h) { router_.head   (path, std::move(mw), std::move(h)); return *this; }
    App& all    (const std::string& path, std::vector<Middleware> mw, Handler h) { router_.all    (path, std::move(mw), std::move(h)); return *this; }

    std::vector<RouteInfo> routes() const { return router_.routes(); }

    // ──────────────────────────────────────────────────────
    //  listen() overloads
    // ──────────────────────────────────────────────────────
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
