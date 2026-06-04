#pragma once

#include "request.h"
#include "response.h"

#include <functional>
#include <cctype>
#include <initializer_list>
#include <regex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace xp {

using Handler = std::function<void(Request&, Response&)>;
using Next = std::function<void()>;
using Middleware = std::function<void(Request&, Response&, Next)>;

struct RouteMatch {
    bool matched = false;
    Handler handler;
    std::vector<Middleware> middleware;
    std::unordered_map<std::string, std::string> params;
};

struct RouteInfo {
    std::string method;
    std::string path;
};

class Router {
private:
    struct Route {
        std::string method;
        std::string path;
        std::regex pattern;
        std::vector<std::string> param_names;
        std::vector<Middleware> middleware;
        Handler handler;
    };

    std::vector<Route> routes_;
    bool case_sensitive_ = true;
    bool strict_trailing_slash_ = true;

    static std::string joinPaths(const std::string& prefix, const std::string& path) {
        if (prefix.empty() || prefix == "/") {
            return path.empty() ? "/" : path;
        }
        if (path.empty() || path == "/") {
            return prefix;
        }
        if (prefix.back() == '/' && path.front() == '/') {
            return prefix + path.substr(1);
        }
        if (prefix.back() != '/' && path.front() != '/') {
            return prefix + "/" + path;
        }
        return prefix + path;
    }

    static std::string escapeRegex(char c) {
        static const std::string special = R"(\.^$|()[]{}+?)";
        if (special.find(c) != std::string::npos) {
            return std::string("\\") + c;
        }
        return std::string(1, c);
    }

    static std::regex compilePath(const std::string& path,
                                  std::vector<std::string>& param_names,
                                  bool case_sensitive,
                                  bool strict_trailing_slash) {
        std::string pattern = "^";

        for (std::size_t i = 0; i < path.size();) {
            if (path[i] == ':') {
                std::size_t start = ++i;
                while (i < path.size() && (std::isalnum(static_cast<unsigned char>(path[i])) || path[i] == '_')) {
                    ++i;
                }
                param_names.push_back(path.substr(start, i - start));
                const bool optional = i < path.size() && path[i] == '?';
                if (optional) {
                    ++i;
                    if (!pattern.empty() && pattern.back() == '/') {
                        pattern.pop_back();
                        pattern += "(?:/([^/]+))?";
                    } else {
                        pattern += "([^/]*)";
                    }
                } else {
                    pattern += "([^/]+)";
                }
                continue;
            }

            if (path[i] == '*') {
                param_names.push_back("wildcard");
                pattern += "(.*)";
                ++i;
                continue;
            }

            pattern += escapeRegex(path[i]);
            ++i;
        }

        if (!strict_trailing_slash && path.size() > 1 && path.back() != '/') {
            pattern += "/?";
        }
        pattern += "$";
        return case_sensitive ? std::regex(pattern)
                              : std::regex(pattern, std::regex_constants::icase);
    }

public:
    Router& caseSensitive(bool enabled) {
        case_sensitive_ = enabled;
        return *this;
    }

    Router& strictTrailingSlash(bool enabled) {
        strict_trailing_slash_ = enabled;
        return *this;
    }

    void add(const std::string& method,
             const std::string& path,
             Handler handler,
             std::vector<Middleware> middleware = {}) {
        std::vector<std::string> param_names;
        routes_.push_back(Route{
            method,
            path,
            compilePath(path, param_names, case_sensitive_, strict_trailing_slash_),
            std::move(param_names),
            std::move(middleware),
            std::move(handler)
        });
    }

    void use(const std::string& prefix, const Router& router) {
        for (const auto& route : router.routes_) {
            add(route.method, joinPaths(prefix, route.path), route.handler, route.middleware);
        }
    }

    void get(const std::string& path, Handler handler) { add("GET", path, std::move(handler)); }
    void post(const std::string& path, Handler handler) { add("POST", path, std::move(handler)); }
    void put(const std::string& path, Handler handler) { add("PUT", path, std::move(handler)); }
    void patch(const std::string& path, Handler handler) { add("PATCH", path, std::move(handler)); }
    void del(const std::string& path, Handler handler) { add("DELETE", path, std::move(handler)); }
    void options(const std::string& path, Handler handler) { add("OPTIONS", path, std::move(handler)); }
    void head(const std::string& path, Handler handler) { add("HEAD", path, std::move(handler)); }
    void all(const std::string& path, Handler handler) { add("*", path, std::move(handler)); }

    void get(const std::string& path, std::vector<Middleware> middleware, Handler handler) { add("GET", path, std::move(handler), std::move(middleware)); }
    void post(const std::string& path, std::vector<Middleware> middleware, Handler handler) { add("POST", path, std::move(handler), std::move(middleware)); }
    void put(const std::string& path, std::vector<Middleware> middleware, Handler handler) { add("PUT", path, std::move(handler), std::move(middleware)); }
    void patch(const std::string& path, std::vector<Middleware> middleware, Handler handler) { add("PATCH", path, std::move(handler), std::move(middleware)); }
    void del(const std::string& path, std::vector<Middleware> middleware, Handler handler) { add("DELETE", path, std::move(handler), std::move(middleware)); }
    void options(const std::string& path, std::vector<Middleware> middleware, Handler handler) { add("OPTIONS", path, std::move(handler), std::move(middleware)); }
    void head(const std::string& path, std::vector<Middleware> middleware, Handler handler) { add("HEAD", path, std::move(handler), std::move(middleware)); }
    void all(const std::string& path, std::vector<Middleware> middleware, Handler handler) { add("*", path, std::move(handler), std::move(middleware)); }

    RouteMatch match(const std::string& method, const std::string& path) const {
        for (const auto& route : routes_) {
            if (route.method != "*" && route.method != method) {
                continue;
            }

            std::smatch matches;
            if (!std::regex_match(path, matches, route.pattern)) {
                continue;
            }

            RouteMatch result;
            result.matched = true;
            result.handler = route.handler;
            result.middleware = route.middleware;
            for (std::size_t i = 0; i < route.param_names.size(); ++i) {
                result.params[route.param_names[i]] = matches[i + 1].str();
            }
            return result;
        }

        return {};
    }

    std::vector<RouteInfo> routes() const {
        std::vector<RouteInfo> items;
        items.reserve(routes_.size());
        for (const auto& route : routes_) {
            items.push_back(RouteInfo{route.method, route.path});
        }
        return items;
    }

    std::vector<std::string> allowedMethods(const std::string& path) const {
        std::vector<std::string> methods;
        for (const auto& route : routes_) {
            std::smatch matches;
            if (std::regex_match(path, matches, route.pattern)) {
                methods.push_back(route.method);
            }
        }
        return methods;
    }
};

} // namespace xp
