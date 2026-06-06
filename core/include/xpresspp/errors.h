#pragma once

// ============================================================
//  xpress++ errors.h
//  Rich typed error hierarchy that gives developers actionable
//  messages when things go wrong — both at request time and
//  at framework-setup time.
// ============================================================

#include <stdexcept>
#include <string>
#include <sstream>
#include <optional>

namespace xp {

// ────────────────────────────────────────────────────────────
//  Base: HttpError
//  Throw this (or a subclass) from any route/middleware handler
//  and xpress++ will automatically convert it into a proper
//  HTTP response.
// ────────────────────────────────────────────────────────────
class HttpError : public std::runtime_error {
private:
    int  status_code_;
    std::string hint_;    // Developer-facing hint shown in debug mode

public:
    HttpError(int status_code, const std::string& message, const std::string& hint = "")
        : std::runtime_error(message)
        , status_code_(status_code)
        , hint_(hint)
    {}

    int statusCode() const noexcept { return status_code_; }
    const std::string& hint()       const noexcept { return hint_; }

    // Allow chaining:  throw xp::HttpError(400, "...").withHint("...")
    HttpError withHint(const std::string& h) const {
        return HttpError(status_code_, what(), h);
    }
};

// ────────────────────────────────────────────────────────────
//  Convenience typed subclasses
// ────────────────────────────────────────────────────────────

/// 400 – malformed request, missing field, bad JSON, etc.
class BadRequestError : public HttpError {
public:
    explicit BadRequestError(const std::string& message,
                             const std::string& hint = "")
        : HttpError(400, message, hint) {}
};

/// 401 – caller must authenticate first
class UnauthorizedError : public HttpError {
public:
    explicit UnauthorizedError(const std::string& message = "Unauthorized",
                               const std::string& hint = "")
        : HttpError(401, message, hint) {}
};

/// 403 – authenticated but not permitted
class ForbiddenError : public HttpError {
public:
    explicit ForbiddenError(const std::string& message = "Forbidden",
                            const std::string& hint = "")
        : HttpError(403, message, hint) {}
};

/// 404 – resource simply does not exist
class NotFoundError : public HttpError {
public:
    explicit NotFoundError(const std::string& message = "Not Found",
                           const std::string& hint = "")
        : HttpError(404, message, hint) {}
};

/// 405 – route exists but not for this HTTP method
class MethodNotAllowedError : public HttpError {
public:
    explicit MethodNotAllowedError(const std::string& method,
                                   const std::string& path)
        : HttpError(405,
                    "Method " + method + " is not allowed for " + path,
                    "Register a handler with app." + toLower(method) +
                    "(\"" + path + "\", ...) or check your router setup.")
    {}

private:
    static std::string toLower(std::string s) {
        for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    }
};

/// 409 – state conflict (e.g. duplicate resource)
class ConflictError : public HttpError {
public:
    explicit ConflictError(const std::string& message,
                           const std::string& hint = "")
        : HttpError(409, message, hint) {}
};

/// 422 – syntactically valid but semantically wrong (validation)
class UnprocessableEntityError : public HttpError {
public:
    explicit UnprocessableEntityError(const std::string& message,
                                      const std::string& hint = "")
        : HttpError(422, message, hint) {}
};

/// 429 – rate-limited
class TooManyRequestsError : public HttpError {
public:
    explicit TooManyRequestsError(const std::string& message = "Too many requests",
                                  const std::string& hint = "")
        : HttpError(429, message, hint) {}
};

/// 500 – unexpected internal failure
class InternalError : public HttpError {
public:
    explicit InternalError(const std::string& message = "Internal server error",
                           const std::string& hint = "")
        : HttpError(500, message, hint) {}
};

/// 503 – service temporarily unavailable (e.g. DB down)
class ServiceUnavailableError : public HttpError {
public:
    explicit ServiceUnavailableError(const std::string& message = "Service unavailable",
                                     const std::string& hint = "")
        : HttpError(503, message, hint) {}
};

// ────────────────────────────────────────────────────────────
//  Framework setup errors (thrown at startup, not per-request)
//  These terminate the process with a clear message.
// ────────────────────────────────────────────────────────────

/// Thrown when the developer misconfigures the App (e.g. bad port, missing cert).
class ConfigurationError : public std::runtime_error {
private:
    std::string fix_;
public:
    ConfigurationError(const std::string& message, const std::string& fix = "")
        : std::runtime_error(message), fix_(fix) {}

    const std::string& fix() const noexcept { return fix_; }
};

/// Thrown when a middleware is registered incorrectly (forgot next(), etc.)
class MiddlewareError : public std::runtime_error {
private:
    std::string fix_;
public:
    MiddlewareError(const std::string& message, const std::string& fix = "")
        : std::runtime_error(message), fix_(fix) {}

    const std::string& fix() const noexcept { return fix_; }
};

// ────────────────────────────────────────────────────────────
//  Convenience throw helpers  (throw xp::abort(404, "..."))
// ────────────────────────────────────────────────────────────

[[noreturn]] inline void abort(int status, const std::string& message,
                                const std::string& hint = "") {
    throw HttpError(status, message, hint);
}

[[noreturn]] inline void badRequest(const std::string& message,
                                     const std::string& hint = "") {
    throw BadRequestError(message, hint);
}

[[noreturn]] inline void unauthorized(const std::string& message = "Unauthorized",
                                       const std::string& hint = "") {
    throw UnauthorizedError(message, hint);
}

[[noreturn]] inline void forbidden(const std::string& message = "Forbidden",
                                    const std::string& hint = "") {
    throw ForbiddenError(message, hint);
}

[[noreturn]] inline void notFound(const std::string& message = "Not Found",
                                   const std::string& hint = "") {
    throw NotFoundError(message, hint);
}

[[noreturn]] inline void conflict(const std::string& message,
                                   const std::string& hint = "") {
    throw ConflictError(message, hint);
}

[[noreturn]] inline void unprocessable(const std::string& message,
                                        const std::string& hint = "") {
    throw UnprocessableEntityError(message, hint);
}

[[noreturn]] inline void tooManyRequests(const std::string& message = "Too many requests",
                                          const std::string& hint = "") {
    throw TooManyRequestsError(message, hint);
}

[[noreturn]] inline void serverError(const std::string& message = "Internal server error",
                                      const std::string& hint = "") {
    throw InternalError(message, hint);
}

} // namespace xp
