#pragma once

#include "../router.h"

#include <functional>
#include <string>
#include <utility>
#include <vector>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/buffer.h>

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

// Public JWT Utility API
inline std::string generateJwt(const std::string& username, const std::string& secret) {
    std::string header = R"({"alg":"HS256","typ":"JWT"})";
    std::string payload = R"({"username":")" + username + R"("})";
    
    std::string encoded_header = detail::base64_url_encode(header);
    std::string encoded_payload = detail::base64_url_encode(payload);
    
    std::string signature_input = encoded_header + "." + encoded_payload;
    std::string signature = detail::base64_url_encode(detail::hmac_sha256(secret, signature_input));
    
    return signature_input + "." + signature;
}

inline bool verifyJwt(const std::string& token, const std::string& secret, std::string& username) {
    size_t dot1 = token.find('.');
    if (dot1 == std::string::npos) return false;
    size_t dot2 = token.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return false;
    
    std::string encoded_header = token.substr(0, dot1);
    std::string encoded_payload = token.substr(dot1 + 1, dot2 - dot1 - 1);
    std::string signature = token.substr(dot2 + 1);
    
    std::string signature_input = encoded_header + "." + encoded_payload;
    std::string expected_signature = detail::base64_url_encode(detail::hmac_sha256(secret, signature_input));
    
    if (signature != expected_signature) return false;
    
    std::string payload = detail::base64_url_decode(encoded_payload);
    size_t user_pos = payload.find(R"("username":")");
    if (user_pos == std::string::npos) return false;
    size_t start = user_pos + 12;
    size_t end = payload.find('"', start);
    if (end == std::string::npos) return false;
    
    username = payload.substr(start, end - start);
    return true;
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
