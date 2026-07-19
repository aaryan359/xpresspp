#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-$ROOT_DIR/build}"
APP_BIN="$BUILD_DIR/xpresspp_sandbox_api"

if [[ ! -x "$APP_BIN" ]]; then
  cmake -S "$ROOT_DIR" -B "$BUILD_DIR"
  cmake --build "$BUILD_DIR" --parallel
fi

export PORT="${PORT:-8080}"
export SANDBOX_RATE_LIMIT="${SANDBOX_RATE_LIMIT:-120}"
export LLM_BASE_URL="${LLM_BASE_URL:-http://127.0.0.1:8081}"
export LLM_API_PATH="${LLM_API_PATH:-/v1/chat/completions}"
export LLM_MODEL="${LLM_MODEL:-xpresspp-qwen2.5-0.5b}"
export LLM_TIMEOUT_SECONDS="${LLM_TIMEOUT_SECONDS:-60}"

echo "[xpress++ sandbox] Starting API on port $PORT"
echo "[xpress++ sandbox] Fixed LLM upstream: $LLM_BASE_URL"

exec "$APP_BIN"
