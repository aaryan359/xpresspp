# Performance Benchmarking & Analysis

Xpress++ is engineered to deliver **bare-metal C++ speed** with the **expressive simplicity of JavaScript/Python**. To demonstrate this in a realistic environment, we executed a rigorous, multi-dimensional benchmarking suite comparing Xpress++ against industry-standard backend frameworks.

---

## Test Environment & Hardware Specs

The benchmarks were conducted locally to ensure isolated execution and avoid network noise.

* **Operating System**: Linux Kernel-based OS (x86_64 architecture)
* **Processor (CPU)**: 16 Logical Cores (optimized thread scheduling)
* **System Memory**: High-performance dual-channel workstation memory
* **Testing Tool**: `wrk` (HTTP benchmarking tool)
* **Load Concurrency**: 400 concurrent connections, using all available CPU threads for load generation
* **Test Duration**: 10 seconds per run with a 2-second warm-up phase

---

## Performance & Resource Metrics

Unlike simple throughput tests, this benchmark evaluates **four dimensions of production readiness**: throughput, average latency, tail latency stability (P99), and memory footprint.

| Framework | Language | Throughput (RPS) | Avg Latency | Tail Latency (P99) | Idle RAM | Peak RAM | CPU Efficiency |
| :--- | :--- | :--- | :--- | :--- | :--- | :--- | :--- |
| **Rust (Axum)** | Rust | **503,412.40** | **0.77 ms** | **2.67 ms** | 3.1 MB | **15.2 MB** | **68,514.3** |
| **Go (net/http)** | Go | **307,879.72** | **1.90 ms** | **10.26 ms** | 6.7 MB | **31.5 MB** | **33,379.2** |
| <mark style="background-color: var(--vp-c-brand-soft); color: var(--vp-c-brand-1); padding: 2px 6px; border-radius: 4px; font-weight: bold;">Xpress++ (C++20)</mark> | **C++20** | **240,066.03** | **1.89 ms** | **8.37 ms** | 15.4 MB | **19.3 MB** | **21,476.4** |
| **Express (Node.js)** | JavaScript | **129,552.23** | **3.52 ms** | **15.38 ms** | 1075.0 MB | **1,398.7 MB** | **10,899.2** |
| **FastAPI (Python)** | Python | **63,582.89** | **6.47 ms** | **19.99 ms** | 781.0 MB | **786.7 MB** | **5,037.2** |

*Note: CPU Efficiency represents the throughput (RPS) served per 100% CPU core utilization. Higher is better.*

---

## Multi-Dimensional Architectural Breakdown

### 1. Memory footprint (Peak RAM)
One of the most drastic differences between C++ and interpreted runtimes is memory consumption.
* **Xpress++** and **Rust** operate natively on the hardware, requiring **under 20 MB of RAM** under extreme concurrency load.
* **Express** and **FastAPI** require clustered processes to scale across CPU cores. Due to the overhead of the Node V8 engine and Python interpreter for each worker, they consume between **780 MB and 1.4 GB of RAM** under identical loads.
* **Xpress++ is 40x to 70x more memory-efficient** than Express and FastAPI, allowing you to run dozens of microservices on a single entry-level cloud instance.

### 2. Tail Latencies & GC Resilience (P99)
In high-throughput systems, median latency (P50) is only half the story. High **P99 latencies** (the worst 1% of requests) degrade user experience.
* **Xpress++** uses native C++ compile-time memory management (RAII) and does not have a Garbage Collector. Latency remains flat and highly predictable (**8.37 ms P99**).
* **Express** and **FastAPI** experience periodic garbage collection sweeps that halt the event loop, causing tail latency to spike to **15ms - 20ms**.

### 3. CPU Efficiency & Event Loop Routing
* **Rust (Axum)** uses a multi-threaded work-stealing event loop (Tokio) that balances tasks dynamically across cores, yielding unmatched I/O forwarding.
* **Xpress++** utilizes Drogon's thread-per-core design with Linux socket port-reuse (`SO_REUSEPORT`). By bypassing standard copy operations using C++ move semantics, Xpress++ achieves a highly scalable architecture that matches Go's latency while remaining far more memory efficient.

---

## Replication Methodology

To run these benchmarks on your local machine, ensure you have the required runtimes (`go`, `cargo`, `npm`, `python3-venv`, and `wrk`) installed, then run the automated script from the project root:

```bash
# Clone the repository and run the automated benchmark runner
./benchmarks/run_benchmarks.sh
```

To run the advanced multi-dimensional resource-monitoring benchmark:

```bash
# Execute the resource tracking benchmark (requires Python 3 + psutil)
python3 benchmarks/advanced_benchmark.py
```

The script will automatically compile C++ and Rust binaries in Release mode with LTO optimizations, launch the servers sequentially, execute the wrk load, monitor RAM/CPU utilization, and output the metrics.
