#pragma once

#include "errors.h"
#include "utils.h"
#include "validation.h"
#include "request.h"
#include "response.h"
#include "router.h"
#include "app.h"
#include "config.h"
#include "env.h"
#include "logger.h"
#include "database.h"
#include "orm.h"
#include "test.h"
#include "websocket.h"
#include "cache.h"
#include "ai.h"
#include "version.h"
#include "db/query.h"
#include "db/renderer.h"
#include "db/transaction.h"
#include "middleware/cors.h"
#include "middleware/rate_limit.h"
#include "middleware/auth.h"
#include "middleware/body_limit.h"
#include "middleware/csrf.h"
#include "middleware/request_id.h"
#include "middleware/security.h"
#include "middleware/session.h"
#include "middleware/validation.h"
#include "middleware/response_time.h"

#if __has_include("db.h")
#include "db.h"
#endif

// Express-like coroutine syntax sugar macros
#define await co_await
#define async -> ::xp::Task<void>
