#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────────────────
# Xpress++ Automated Comparative Benchmark Suite
# ──────────────────────────────────────────────────────────────────────────────
set -e

# Colors
GREEN="\033[32m"; CYAN="\033[36m"; YELLOW="\033[33m"; RED="\033[31m"; BOLD="\033[1m"; RESET="\033[0m"

echo -e "${CYAN}${BOLD}=== Xpress++ Comparative Benchmark Runner ===${RESET}"

# Resolve script directory and change to it
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "${SCRIPT_DIR}"

# Verify wrk is installed
if ! command -v wrk &>/dev/null; then
    echo -e "${RED}Error: 'wrk' is not installed. Please install it first:${RESET}"
    echo "  Ubuntu/Debian: sudo apt install wrk"
    echo "  macOS:         brew install wrk"
    exit 1
fi

RESULTS_FILE="${SCRIPT_DIR}/results.md"
echo -e "# System Benchmarking Results\n" > "$RESULTS_FILE"
echo -e "Generated automatically on $(date)\n" >> "$RESULTS_FILE"
echo -e "| Framework | Language | Endpoint | Requests/Sec | Latency |" >> "$RESULTS_FILE"
echo -e "| :--- | :--- | :--- | :--- | :--- |" >> "$RESULTS_FILE"

# Clean up any leftover processes on script exit or interruption
cleanup() {
    echo -e "\n${YELLOW}▸ Cleaning up background processes...${RESET}"
    kill $XP_PID 2>/dev/null || true
    if [ ! -z "${NODE_PID+x}" ]; then kill $NODE_PID 2>/dev/null || true; fi
    if [ ! -z "${PY_PID+x}" ]; then kill $PY_PID 2>/dev/null || true; fi
    if [ ! -z "${GO_PID+x}" ]; then kill $GO_PID 2>/dev/null || true; fi
    if [ ! -z "${RUST_PID+x}" ]; then kill $RUST_PID 2>/dev/null || true; fi
}
trap cleanup EXIT INT TERM

# Helper to run wrk and extract Requests/sec
run_wrk_test() {
    local port=$1
    local path=$2
    local framework=$3
    local lang=$4

    echo -e "  Testing ${framework} [${path}] on port ${port}..."
    sleep 2 # Let server bind properly

    # Warmup
    wrk -t2 -c50 -d2s http://127.0.0.1:${port}${path} >/dev/null 2>&1

    # Real run
    local num_cores
    num_cores=$(nproc 2>/dev/null || echo 4)
    local output
    output=$(wrk -t"${num_cores}" -c400 -d5s http://127.0.0.1:${port}${path})
    
    local rps
    rps=$(echo "$output" | grep "Requests/sec:" | awk '{print $2}')
    local latency
    latency=$(echo "$output" | grep "Latency" | head -n1 | awk '{print $2}')

    echo -e "    ${GREEN}✓ ${rps} req/sec | Latency: ${latency}${RESET}"
    
    # Save to file
    echo "| ${framework} | ${lang} | \`${path}\` | **${rps}** | ${latency} |" >> "$RESULTS_FILE"
}

# ──────────────────────────────────────────────────────────────────────────────
# 1. Build C++ Xpress++ Server
# ──────────────────────────────────────────────────────────────────────────────
echo -e "\n${CYAN}▸ Building Xpress++ Benchmark Server...${RESET}"
mkdir -p xpresspp/build
cd xpresspp/build
cmake -DCMAKE_BUILD_TYPE=Release .. >/dev/null
make -j$(nproc 2>/dev/null || echo 4) >/dev/null
cd ../..

# ──────────────────────────────────────────────────────────────────────────────
# 2. Build Go Server
# ──────────────────────────────────────────────────────────────────────────────
if command -v go &>/dev/null; then
    echo -e "\n${CYAN}▸ Building Go Benchmark Server...${RESET}"
    cd go
    go build -o go_benchmark main.go
    cd ..
fi

# ──────────────────────────────────────────────────────────────────────────────
# 3. Build Rust Server
# ──────────────────────────────────────────────────────────────────────────────
if command -v cargo &>/dev/null; then
    echo -e "\n${CYAN}▸ Building Rust (Axum) Benchmark Server...${RESET}"
    cd rust
    cargo build --release --quiet
    cd ..
fi

# ──────────────────────────────────────────────────────────────────────────────
# 4. Setup Python Virtual Environment for FastAPI
# ──────────────────────────────────────────────────────────────────────────────
if command -v python3 &>/dev/null; then
    echo -e "\n${CYAN}▸ Setting up Python virtual environment for FastAPI...${RESET}"
    if [ ! -d "fastapi/venv" ]; then
        python3 -m venv fastapi/venv
        ./fastapi/venv/bin/pip install --quiet fastapi uvicorn[standard]
    fi
fi

# ──────────────────────────────────────────────────────────────────────────────
# RUN TESTS
# ──────────────────────────────────────────────────────────────────────────────

# Test 1: Xpress++
echo -e "\n${CYAN}▸ Running Xpress++ Server...${RESET}"
./xpresspp/build/xpresspp_benchmark >/dev/null 2>&1 &
XP_PID=$!
run_wrk_test 8085 "/json" "Xpress++" "C++20"
run_wrk_test 8085 "/text" "Xpress++" "C++20"
kill $XP_PID 2>/dev/null || true
wait $XP_PID 2>/dev/null || true

# Test 2: Rust (Axum)
if command -v cargo &>/dev/null; then
    echo -e "\n${CYAN}▸ Running Rust (Axum) Server...${RESET}"
    ./rust/target/release/rust_benchmark >/dev/null 2>&1 &
    RUST_PID=$!
    run_wrk_test 6000 "/json" "Rust (Axum)" "Rust"
    run_wrk_test 6000 "/text" "Rust (Axum)" "Rust"
    kill $RUST_PID 2>/dev/null || true
    wait $RUST_PID 2>/dev/null || true
fi

# Test 3: Go (net/http)
if command -v go &>/dev/null; then
    echo -e "\n${CYAN}▸ Running Go (net/http) Server...${RESET}"
    ./go/go_benchmark >/dev/null 2>&1 &
    GO_PID=$!
    run_wrk_test 5000 "/json" "Go (net/http)" "Go"
    run_wrk_test 5000 "/text" "Go (net/http)" "Go"
    kill $GO_PID 2>/dev/null || true
    wait $GO_PID 2>/dev/null || true
fi

# Test 4: Express.js
if command -v node &>/dev/null; then
    echo -e "\n${CYAN}▸ Running Express.js Server...${RESET}"
    if [ ! -d "express/node_modules" ]; then
        cd express && npm install --silent >/dev/null 2>&1 && cd ..
    fi
    node express/app.js >/dev/null 2>&1 &
    NODE_PID=$!
    run_wrk_test 3000 "/json" "Express" "JavaScript"
    run_wrk_test 3000 "/text" "Express" "JavaScript"
    kill $NODE_PID 2>/dev/null || true
    wait $NODE_PID 2>/dev/null || true
fi

# Test 5: FastAPI
if [ -d "fastapi/venv" ]; then
    echo -e "\n${CYAN}▸ Running FastAPI (Python) Server...${RESET}"
    cd fastapi
    num_cores=$(nproc 2>/dev/null || echo 4)
    ./venv/bin/uvicorn main:app --port 8000 --workers "${num_cores}" --loop uvloop --http httptools --log-level warning >/dev/null 2>&1 &
    PY_PID=$!
    cd ..
    run_wrk_test 8000 "/json" "FastAPI" "Python"
    run_wrk_test 8000 "/text" "FastAPI" "Python"
    kill $PY_PID 2>/dev/null || true
    wait $PY_PID 2>/dev/null || true
fi

echo -e "\n${GREEN}${BOLD}=== Benchmarks completed successfully! ===${RESET}"
echo -e "Results written to ${BOLD}benchmarks/results.md${RESET}\n"

cat "$RESULTS_FILE"
