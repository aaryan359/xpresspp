---
layout: home

hero:
  name: "Xpress++"
  text: "Dragon speed.\nExpress simplicity."
  tagline: Build blazing-fast C++ web servers with a familiar, comfortable API — powered by Drogon under the hood.
  actions:
    - theme: brand
      text: Get Started →
      link: /getting-started
    - theme: alt
      text: View on GitHub
      link: https://github.com/aaryan359/xpresspp

features:
  - icon: ⚡
    title: Drogon-powered performance
    details: Built on top of Drogon, one of the fastest HTTP frameworks in the world. Handles thousands of requests per second out of the box.
  - icon: 🎯
    title: Express-style API
    details: If you've used Express.js, you already know Xpress++. Routes, middleware, and responses all work exactly the way you'd expect.
  - icon: 🛡️
    title: Rich error handling
    details: Typed HTTP errors, developer hints in debug mode, and colored terminal output so you always know exactly what went wrong.
  - icon: 🧱
    title: Built-in middleware
    details: CORS, rate limiting, authentication, CSRF, security headers, sessions, request IDs, and body limits — all included, zero config required.
  - icon: 🔧
    title: First-class CLI
    details: Create, build, run, and watch your projects with a single command. The xp CLI handles CMake so you don't have to.
  - icon: 📦
    title: Modern C++20
    details: Uses lambdas, std::optional, std::filesystem, structured bindings, and other C++20 features for clean, expressive code.
---

<div class="benchmark-section">

## Blazing Fast Performance

Xpress++ delivers **C++ speed with Express simplicity**. Here is how Xpress++ performs in a comparative benchmark against Go, Rust, Node.js (Express), and Python (FastAPI) on a standard 8-core CPU (handling 200 concurrent connections, 4 threads, using `wrk`).

| Framework | Language | Requests / Sec | Avg Latency | Tail Latency (P99) | Peak RAM |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **Rust (Axum)** | Rust | **503,412** | **0.77 ms** | **2.67 ms** | **15.2 MB** |
| **Go (net/http)** | Go | **307,879** | **1.90 ms** | **10.26 ms** | **31.5 MB** |
| **Xpress++** | C++20 | **240,066** | **1.89 ms** | **8.37 ms** | **19.3 MB** |
| **Express (Node.js)** | JavaScript | **129,552** | **3.52 ms** | **15.38 ms** | **1398.7 MB** |
| **FastAPI** | Python | **63,582** | **6.47 ms** | **19.99 ms** | **786.7 MB** |

::: tip Why is Xpress++ so fast?
Xpress++ compiles to a native binary using Drogon's non-blocking, asynchronous I/O model and thread-pool-based event loop. It delivers high throughput and extremely low latencies, outperforming standard JS/Python frameworks by **300%+** while keeping resource usage minimal.
:::

</div>

<style>
.benchmark-section {
  max-width: 864px;
  margin: 64px auto 0;
  padding: 0 24px;
}
.benchmark-section h2 {
  text-align: center;
  font-size: 2.25rem;
  font-weight: 800;
  letter-spacing: -0.025em;
  margin-bottom: 32px;
  background: linear-gradient(135deg, #a78bfa 0%, #8b5cf6 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
}
.benchmark-section table {
  width: 100%;
  border-collapse: collapse;
  margin: 24px 0;
  font-size: 1rem;
}
.benchmark-section th {
  background-color: var(--vp-c-bg-soft);
  font-weight: 600;
}
.benchmark-section tr:hover {
  background-color: var(--vp-c-bg-mute);
}
</style>

