#pragma once

// utils.h — Re-exports the throw helpers from errors.h for backwards
// compatibility.  All the actual typed error classes live in errors.h.
#include "errors.h"

// Nothing else needed here; xp::badRequest(), xp::notFound(), etc.
// are already defined as [[noreturn]] inline functions in errors.h.
