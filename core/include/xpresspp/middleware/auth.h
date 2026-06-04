#pragma once

#include "../router.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace xp {

using ApiKeyVerifier = std::function<bool(const std::string&)>;
using BasicAuthVerifier = std::function<bool(const std::string&, const std::string&)>;

inline std::string decodeBasicAuthToken(const std::string& input) {
    static const std::string alphabet =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string output;
    int value = 0;
    int bits = -8;
    for (unsigned char c : input) {
        if (c == '=') {
            break;
        }
        const auto index = alphabet.find(static_cast<char>(c));
        if (index == std::string::npos) {
            return "";
        }
        value = (value << 6) + static_cast<int>(index);
        bits += 6;
        if (bits >= 0) {
            output.push_back(static_cast<char>((value >> bits) & 0xff));
            bits -= 8;
        }
    }
    return output;
}

inline Middleware apiKeyAuth(std::string expected_key,
                             std::string header = "x-api-key") {
    return [expected_key = std::move(expected_key), header = std::move(header)](Request& req, Response& res, Next next) {
        if (req.header(header) != expected_key) {
            res.unauthorized("Invalid API key.");
            return;
        }
        next();
    };
}

inline Middleware apiKeyAuth(ApiKeyVerifier verifier,
                             std::string header = "x-api-key") {
    return [verifier = std::move(verifier), header = std::move(header)](Request& req, Response& res, Next next) {
        if (!verifier(req.header(header))) {
            res.unauthorized("Invalid API key.");
            return;
        }
        next();
    };
}

inline Middleware basicAuth(BasicAuthVerifier verifier) {
    return [verifier = std::move(verifier)](Request& req, Response& res, Next next) {
        const auto auth = req.header("authorization");
        const std::string prefix = "Basic ";
        if (auth.rfind(prefix, 0) != 0) {
            res.header("WWW-Authenticate", "Basic").unauthorized("Missing basic auth credentials.");
            return;
        }

        const auto token = decodeBasicAuthToken(auth.substr(prefix.size()));
        const auto separator = token.find(':');
        if (separator == std::string::npos || !verifier(token.substr(0, separator), token.substr(separator + 1))) {
            res.header("WWW-Authenticate", "Basic").unauthorized("Invalid basic auth credentials.");
            return;
        }

        next();
    };
}

inline Middleware requireRole(std::function<bool(Request&)> predicate,
                              std::string message = "Forbidden") {
    return [predicate = std::move(predicate), message = std::move(message)](Request& req, Response& res, Next next) {
        if (!predicate(req)) {
            res.forbidden(message);
            return;
        }
        next();
    };
}

} // namespace xp
