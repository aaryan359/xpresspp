#pragma once

#include "errors.h"
#include "validation.h"
#include "request.h"
#include "response.h"
#include "router.h"
#include "app.h"
#include "config.h"
#include "env.h"
#include "logger.h"
#include "utils.h"
#include "database.h"
#include "orm.h"
#include "test.h"
#include "middleware/cors.h"
#include "middleware/rate_limit.h"
#include "middleware/auth.h"
#include "middleware/body_limit.h"
#include "middleware/csrf.h"
#include "middleware/request_id.h"
#include "middleware/security.h"
#include "middleware/session.h"
#include "middleware/validation.h"

#if __has_include("db.h")
#include "db.h"
#endif

// Express-like coroutine syntax sugar macros
#define await co_await
#define async -> ::xp::Task<void>




