#pragma once

#include "../router.h"

#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>
#include <json/json.h>

namespace xp {

using ApiKeyVerifier = std::function<bool(const std::string&)>;
using BasicAuthVerifier = std::function<bool(const std::string&, const std::string&)>;

// Helper functions for JWT (Base64 encoding/decoding and signing)
namespace detail {

inline std::string base64_encode(const std::string& input) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_write(bio, input.c_str(), input.length());
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    BIO_set_close(bio, BIO_NOCLOSE);
    
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);
    return result;
}

inline std::string base64_url_encode(const std::string& input) {
    std::string b64 = base64_encode(input);
    std::string b64url = "";
    for (char c : b64) {
        if (c == '+') b64url += '-';
        else if (c == '/') b64url += '_';
        else if (c == '=') continue;
        else b64url += c;
    }
    return b64url;
}

inline std::string base64_decode(const std::string& input) {
    BIO *bio, *b64;
    std::string result(input.length(), '\0');

    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new_mem_buf(input.data(), input.length());
    bio = BIO_push(b64, bio);
    int len = BIO_read(bio, &result[0], input.length());
    if (len < 0) len = 0;
    result.resize(len);
    BIO_free_all(bio);
    return result;
}

inline std::string base64_url_decode(const std::string& input) {
    std::string b64 = input;
    for (char& c : b64) {
        if (c == '-') c = '+';
        else if (c == '_') c = '/';
    }
    while (b64.length() % 4 != 0) {
        b64 += '=';
    }
    return base64_decode(b64);
}

inline std::string hmac_sha256(const std::string& secret, const std::string& data) {
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int len = 0;
    
    HMAC(EVP_sha256(), secret.data(), secret.length(), 
         reinterpret_cast<const unsigned char*>(data.data()), data.length(), 
         hash, &len);
         
    return std::string(reinterpret_cast<char*>(hash), len);
}

} // namespace detail

// ── Public JWT Utility API ───────────────────────────────────

// Generate a JWT with a custom claims object.
// ttlSeconds: token lifetime in seconds (default 1 hour). Pass 0 for no expiry.
inline std::string generateJwt(const Json::Value& claims,
                                const std::string& secret,
                                int ttlSeconds = 3600) {
    const std::string header = R"({"alg":"HS256","typ":"JWT"})";

    Json::Value payload = claims;
    const auto now = static_cast<Json::Int64>(std::time(nullptr));
    payload["iat"] = now;
    if (ttlSeconds > 0) {
        payload["exp"] = now + static_cast<Json::Int64>(ttlSeconds);
    }

    Json::StreamWriterBuilder wb;
    wb["indentation"] = "";
    const std::string payload_str = Json::writeString(wb, payload);

    const std::string encoded_header  = detail::base64_url_encode(header);
    const std::string encoded_payload = detail::base64_url_encode(payload_str);
    const std::string sig_input       = encoded_header + "." + encoded_payload;
    const std::string signature       = detail::base64_url_encode(
        detail::hmac_sha256(secret, sig_input));

    return sig_input + "." + signature;
}

// Convenience overload matching the old signature — embeds {"sub", "username"} claim.
inline std::string generateJwt(const std::string& username,
                                const std::string& secret,
                                int ttlSeconds = 3600) {
    Json::Value claims;
    claims["sub"]      = username;
    claims["username"] = username;
    return generateJwt(claims, secret, ttlSeconds);
}

// Verify a JWT token. Returns false if invalid, tampered, or expired.
// Populates `username` from the "username" or "sub" claim on success.
inline bool verifyJwt(const std::string& token,
                      const std::string& secret,
                      std::string&       username) {
    const auto dot1 = token.find('.');
    if (dot1 == std::string::npos) return false;
    const auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return false;

    const std::string encoded_header  = token.substr(0, dot1);
    const std::string encoded_payload = token.substr(dot1 + 1, dot2 - dot1 - 1);
    const std::string signature       = token.substr(dot2 + 1);

    const std::string sig_input          = encoded_header + "." + encoded_payload;
    const std::string expected_signature = detail::base64_url_encode(
        detail::hmac_sha256(secret, sig_input));
    if (signature != expected_signature) return false;

    // Parse payload JSON
    const std::string payload_str = detail::base64_url_decode(encoded_payload);
    Json::Value payload;
    Json::CharReaderBuilder rb;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(rb.newCharReader());
    if (!reader->parse(payload_str.data(),
                       payload_str.data() + payload_str.size(),
                       &payload, &errs)) {
        return false;
    }

    // Check expiry
    if (payload.isMember("exp") && payload["exp"].isIntegral()) {
        const auto exp = static_cast<std::time_t>(payload["exp"].asInt64());
        if (std::time(nullptr) > exp) return false;
    }

    // Extract username
    if (payload.isMember("username") && payload["username"].isString()) {
        username = payload["username"].asString();
    } else if (payload.isMember("sub") && payload["sub"].isString()) {
        username = payload["sub"].asString();
    } else {
        return false;
    }

    return true;
}

// Decode and return the full verified claims object.
// Returns Json::Value() (null) on any failure (bad signature, expired, etc.).
inline Json::Value decodeJwt(const std::string& token, const std::string& secret) {
    const auto dot1 = token.find('.');
    if (dot1 == std::string::npos) return {};
    const auto dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return {};

    const std::string encoded_header  = token.substr(0, dot1);
    const std::string encoded_payload = token.substr(dot1 + 1, dot2 - dot1 - 1);
    const std::string signature       = token.substr(dot2 + 1);

    const std::string sig_input          = encoded_header + "." + encoded_payload;
    const std::string expected_signature = detail::base64_url_encode(
        detail::hmac_sha256(secret, sig_input));
    if (signature != expected_signature) return {};

    const std::string payload_str = detail::base64_url_decode(encoded_payload);
    Json::Value payload;
    Json::CharReaderBuilder rb;
    std::string errs;
    std::unique_ptr<Json::CharReader> reader(rb.newCharReader());
    if (!reader->parse(payload_str.data(),
                       payload_str.data() + payload_str.size(),
                       &payload, &errs)) {
        return {};
    }

    if (payload.isMember("exp") && payload["exp"].isIntegral()) {
        const auto exp = static_cast<std::time_t>(payload["exp"].asInt64());
        if (std::time(nullptr) > exp) return {};
    }

    return payload;
}

// Built-in JWT Middleware
inline Middleware jwtAuth(std::string secret) {
    return [secret = std::move(secret)](Request& req, Response& res, Next next) {
        std::string auth_header = req.header("authorization");
        if (auth_header.rfind("Bearer ", 0) == 0) {
            std::string token = auth_header.substr(7);
            std::string username;
            if (verifyJwt(token, secret, username)) {
                req.setParam("username", username);
                next();
                return;
            }
        }
        res.status(401).json({{"error", "Unauthorized: Invalid or missing token"}});
    };
}

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
