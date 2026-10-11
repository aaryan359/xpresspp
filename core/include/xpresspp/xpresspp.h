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
#include "version.h"

#include "middleware/cors.h"
#include "middleware/rate_limit.h"
#include "middleware/body_limit.h"
#include "middleware/request_id.h"
#include "middleware/security.h"

// Express-like coroutine syntax sugar macros
#define await co_await
#define async -> ::xp::Task<void>
