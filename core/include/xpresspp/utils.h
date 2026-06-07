#pragma once

// utils.h — Re-exports the throw helpers from errors.h for backwards
// compatibility.  All the actual typed error classes live in errors.h.
#include "errors.h"

// Express-like coroutine syntax sugar macros
#define await co_await
#define async -> ::xp::Task
