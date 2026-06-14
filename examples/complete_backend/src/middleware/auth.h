#pragma once
#include <xpresspp/xpresspp.h>
#include <string>

class AuthMiddleware {
public:
    // Shared JWT Secret key for token generation and verification
    static std::string getSecret() {
        return "xpresspp-jwt-secret-key-12345";
    }

    // Returns a middleware that validates JWT bearer tokens in the authorization header
    static xp::Middleware requireAuth() {
        return xp::jwtAuth(getSecret());
    }
};
