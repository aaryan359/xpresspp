#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MODEL_CACHE_DIR="${MODEL_CACHE_DIR:-$ROOT_DIR/models}"
LLAMA_BIN="${LLAMA_BIN:-llama-server}"
HOST="${LLM_HOST:-127.0.0.1}"
PORT="${LLM_PORT:-8081}"
ALIAS="${LLM_MODEL:-xpresspp-qwen2.5-0.5b}"

mkdir -p "$MODEL_CACHE_DIR"

echo "[xpress++ sandbox] Starting fixed demo LLM"
echo "[xpress++ sandbox] Model: Qwen/Qwen2.5-0.5B-Instruct-GGUF:Q4_K_M"
echo "[xpress++ sandbox] Cache: $MODEL_CACHE_DIR"
echo "[xpress++ sandbox] URL: http://$HOST:$PORT"

cd "$MODEL_CACHE_DIR"

exec "$LLAMA_BIN" \
  -hf Qwen/Qwen2.5-0.5B-Instruct-GGUF:Q4_K_M \
  --host "$HOST" \
  --port "$PORT" \
  --alias "$ALIAS" \
  -c "${LLM_CONTEXT:-2048}" \
  -n "${LLM_PREDICT:-160}" \
  -t "${LLM_THREADS:-4}" \
  --parallel "${LLM_PARALLEL:-1}" \
  --mlock
