#pragma once

#include "app.h"

#include <string>

namespace xp {

inline HttpError badRequest(const std::string& message) {
    return HttpError(400, message);
}

inline HttpError unauthorized(const std::string& message = "Unauthorized") {
    return HttpError(401, message);
}

inline HttpError forbidden(const std::string& message = "Forbidden") {
    return HttpError(403, message);
}

inline HttpError notFound(const std::string& message = "Not found") {
    return HttpError(404, message);
}

inline HttpError serverError(const std::string& message = "Internal server error") {
    return HttpError(500, message);
}

} // namespace xp
