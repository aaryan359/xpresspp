---
layout: home

hero:
  name: "Xpress++"
  text: "Fast C++ APIs for web, data, and native AI."
  tagline: Build Drogon-powered services with an Express-like API, built-in middleware, SQL tooling, and lightweight helpers for exposing C++ AI workloads.
  actions:
    - theme: brand
      text: Get Started →
      link: /getting-started
    - theme: alt
      text: AI Interface
      link: /ai
    - theme: alt
      text: Performance Benchmarks
      link: /benchmarks
    - theme: alt
      text: View on GitHub
      link: https://github.com/aaryan359/xpresspp
---

<!-- Visual Stats Dashboard -->
<div class="dashboard-container">
  <div class="dashboard-header">
    <h2>Empowered by C++ Hardware Performance</h2>
    <p class="subtitle">Production metrics under peak concurrent load (400 concurrent connections, 16-core CPU)</p>
  </div>
  <div class="stats-grid">
    <div class="stat-card highlighted">
      <div class="glow-effect"></div>
      <span class="stat-num">250K+</span>
      <span class="stat-label">Requests / Second</span>
      <span class="stat-detail">Saturates multi-core hardware</span>
    </div>
    <div class="stat-card">
      <span class="stat-num">1.89ms</span>
      <span class="stat-label">Average Latency</span>
      <span class="stat-detail">Flat response times</span>
    </div>
    <div class="stat-card">
      <span class="stat-num">19.3MB</span>
      <span class="stat-label">Peak Memory Usage</span>
      <span class="stat-detail">microscopic RAM footprint</span>
    </div>
    <div class="stat-card">
      <span class="stat-num">70x</span>
      <span class="stat-label">Memory Efficient</span>
      <span class="stat-detail">VS Express & FastAPI</span>
    </div>
  </div>
</div>

<!-- Detailed Callout / Installation Command (Moved Between Stats & Code Blocks) -->
<div class="cta-banner">
  <h3>Wrap native C++ workloads in clean HTTP APIs</h3>
  <p>Use Xpress++ for API services, SQL-backed backends, and local AI runtimes such as llama.cpp, ONNX Runtime, Whisper.cpp, or custom C++ inference.</p>
  <div class="install-command">
    <code>curl -fsSL https://raw.githubusercontent.com/aaryan359/xpresspp/main/install.sh | bash</code>
  </div>
</div>

<!-- Side-by-Side API Comparison -->
<div class="comparison-container">
  <div class="comparison-header">
    <h2>Familiar API, Native Runtime</h2>
    <p class="subtitle">Keep the route shape readable while your hot path stays close to Drogon, C++ libraries, and native model runtimes.</p>
  </div>
  
  <div class="code-wrapper">

::: code-group

```cpp [main.cpp (Xpress++)]
#include <xpresspp/xpresspp.h>

int main() {
    xp::App app;

    app.post("/chat", xp::ai::chat([](const xp::ai::ChatRequest& input) {
        return "Native model reply for: " + input.message;
    }));

    app.get("/api/user", [](xp::Request&, xp::Response& res) {
        res.ok({
            {"status", "success"},
            {"user", "Aaryan"}
        });
    });

    app.listen(3000);
    return 0;
}
```

```javascript [server.js (Express)]
const express = require('express');
const app = express();

// Familiar route shape
app.get('/api/user', (req, res) => {
    res.json({
        status: 'success',
        user: 'Aaryan'
    });
});

app.listen(3000);
```

:::

  </div>
</div>

<!-- Feature Highlights Section (Moved to Bottom) -->
<div class="features-section">
  <div class="features-heading">
    <h2>What You Can Build Today</h2>
    <p class="subtitle">The alpha focuses on the pieces that are already useful: HTTP APIs, middleware, SQL-backed services, and native C++ workloads.</p>
  </div>
  <div class="features-grid">
    <div class="feature-card">
      <span class="feature-icon">HTTP</span>
      <h3 class="feature-title">Drogon-powered performance</h3>
      <p class="feature-detail">Built on top of Drogon, one of the fastest HTTP frameworks in the world. Handles thousands of requests per second out of the box.</p>
    </div>
    <div class="feature-card">
      <span class="feature-icon">API</span>
      <h3 class="feature-title">Express-style API</h3>
      <p class="feature-detail">If you've used Express.js, you already know Xpress++. Routes, middleware, and responses all work exactly the way you'd expect.</p>
    </div>
    <div class="feature-card">
      <span class="feature-icon">AI</span>
      <h3 class="feature-title">Native AI interface</h3>
      <p class="feature-detail">Expose C++ inference code as JSON APIs with <code>xp::ai::chat</code>. Bring your own llama.cpp, ONNX, OpenCV, Whisper, or custom model layer.</p>
    </div>
    <div class="feature-card">
      <span class="feature-icon">SQL</span>
      <h3 class="feature-title">Generated SQL client</h3>
      <p class="feature-detail">Use <code>schema.xp</code>, migrations, and the generated <code>xpd</code> client for PostgreSQL, SQLite, and MySQL-backed services.</p>
    </div>
    <div class="feature-card">
      <span class="feature-icon">ERR</span>
      <h3 class="feature-title">Rich error handling</h3>
      <p class="feature-detail">Typed HTTP errors, developer hints in debug mode, and colored terminal output so you always know exactly what went wrong.</p>
    </div>
    <div class="feature-card">
      <span class="feature-icon">MW</span>
      <h3 class="feature-title">Built-in middleware</h3>
      <p class="feature-detail">CORS, rate limiting, authentication, CSRF, security headers, sessions, request IDs, and body limits — all included, zero config required.</p>
    </div>
    <div class="feature-card">
      <span class="feature-icon">C++20</span>
      <h3 class="feature-title">Modern C++20</h3>
      <p class="feature-detail">Uses lambdas, std::optional, std::filesystem, structured bindings, and other C++20 features for clean, expressive code.</p>
    </div>
    <div class="feature-card">
      <span class="feature-icon">CLI</span>
      <h3 class="feature-title">First-class CLI</h3>
      <p class="feature-detail">Create, build, run, and watch your projects with a single command. The xp CLI handles CMake so you don't have to.</p>
    </div>
  </div>
</div>

<style>
/* Remove Hero bottom border divider */
.VPHero {
  border-bottom: none !important;
}

/* Dashboard Styling */
.dashboard-container, .comparison-container, .features-section {
  max-width: 1152px;
  margin: 80px auto 0;
  padding: 0 24px;
}

.dashboard-header, .comparison-header, .features-heading {
  text-align: center;
  margin-bottom: 40px;
}

.dashboard-header h2, .comparison-header h2, .features-heading h2 {
  font-size: 2.25rem;
  font-weight: 800;
  letter-spacing: -0.03em;
  margin-bottom: 8px;
  background: linear-gradient(135deg, var(--vp-c-brand-1) 0%, var(--vp-c-brand-2) 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  border-top: none !important;
  border-bottom: none !important;
  padding-top: 0 !important;
  margin-top: 0 !important;
}

.subtitle {
  color: var(--vp-c-text-2);
  font-size: 1.1rem;
  max-width: 600px;
  margin: 0 auto;
  line-height: 1.5;
}

/* Stats Cards Grid */
.stats-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 24px;
}

.stat-card {
  position: relative;
  background: var(--vp-c-bg-alt);
  border: 1px solid var(--vp-c-border);
  border-radius: 16px;
  padding: 32px 24px;
  text-align: center;
  overflow: hidden;
  transition: all 0.2s ease;
}

.stat-card:hover {
  transform: translateY(-4px);
  border-color: var(--vp-c-brand-3);
  box-shadow: 0 12px 32px rgba(13, 148, 136, 0.08);
}

.stat-card.highlighted {
  border-color: var(--vp-c-brand-1);
  background: color-mix(in srgb, var(--vp-c-brand-soft) 40%, var(--vp-c-bg-alt));
}

.glow-effect {
  position: absolute;
  top: -50%;
  left: -50%;
  width: 200%;
  height: 200%;
  background: radial-gradient(circle, rgba(94, 234, 212, 0.08) 0%, transparent 60%);
  pointer-events: none;
}

.stat-num {
  display: block;
  font-size: 3rem;
  font-weight: 800;
  letter-spacing: -0.05em;
  line-height: 1;
  margin-bottom: 8px;
  color: var(--vp-c-brand-1);
}

.stat-label {
  display: block;
  font-weight: 700;
  font-size: 1.05rem;
  color: var(--vp-c-text-1);
  margin-bottom: 4px;
}

.stat-detail {
  font-size: 0.85rem;
  color: var(--vp-c-text-3);
  text-transform: uppercase;
  letter-spacing: 0.05em;
}

/* Code Comparison Wrapper */
.code-wrapper {
  margin-top: 32px;
  max-width: 864px;
  margin-left: auto;
  margin-right: auto;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.08);
  border-radius: 12px;
  overflow: hidden;
  border: 1px solid var(--vp-c-border);
}

.dark .code-wrapper {
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.32);
}

/* CTA Banner */
.cta-banner {
  max-width: 864px;
  margin: 64px auto 0;
  background: linear-gradient(135deg, var(--vp-c-bg-alt) 0%, var(--vp-c-bg-soft) 100%);
  border: 1px solid var(--vp-c-border);
  border-radius: 20px;
  padding: 40px;
  text-align: center;
  box-shadow: 0 4px 24px rgba(0, 0, 0, 0.04);
}

.cta-banner h3 {
  font-size: 1.5rem;
  font-weight: 700;
  margin-bottom: 8px;
  color: var(--vp-c-text-1);
}

.cta-banner p {
  color: var(--vp-c-text-2);
  margin-bottom: 24px;
}

.install-command {
  background: var(--vp-c-bg);
  border: 1px solid var(--vp-c-border);
  border-radius: 8px;
  padding: 12px 20px;
  display: inline-block;
  font-family: var(--vp-font-family-mono);
  font-size: 0.95rem;
  color: var(--vp-c-brand-1);
}

/* Features Grid Styling */
.features-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(320px, 1fr));
  gap: 24px;
  margin-bottom: 80px;
}

.feature-card {
  position: relative;
  background: var(--vp-c-bg-alt);
  border: 1px solid var(--vp-c-border);
  border-radius: 8px;
  padding: 28px;
  transition: all 0.2s ease;
}

.feature-card:hover {
  transform: translateY(-2px);
  border-color: var(--vp-c-brand-3);
  box-shadow: 0 8px 32px rgba(13, 148, 136, 0.08);
}

.dark .feature-card:hover {
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.32);
}

.feature-icon {
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 44px;
  height: 28px;
  margin-bottom: 16px;
  padding: 0 10px;
  border: 1px solid color-mix(in srgb, var(--vp-c-brand-1) 36%, var(--vp-c-border));
  border-radius: 999px;
  color: var(--vp-c-brand-1);
  background: var(--vp-c-brand-soft);
  font-family: var(--vp-font-family-mono);
  font-size: 0.72rem;
  font-weight: 700;
  letter-spacing: 0;
}

.feature-title {
  font-size: 1.15rem;
  font-weight: 700;
  margin-top: 0 !important;
  margin-bottom: 8px;
  color: var(--vp-c-text-1);
}

.feature-detail {
  font-size: 0.9rem;
  color: var(--vp-c-text-2);
  line-height: 1.6;
  margin: 0;
}

@media (max-width: 768px) {
  .stats-grid {
    grid-template-columns: 1fr 1fr;
  }
}
</style>
