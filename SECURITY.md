# Security Policy

Xpress++ is currently pre-release software. Do not expose an application to the
public internet without reviewing its configuration and keeping Drogon, OpenSSL,
the compiler, and database client libraries updated.

## Reporting a vulnerability

Do not open a public issue containing exploit details. Report suspected security
problems privately to the project maintainers with the affected version, a minimal
reproduction, impact, and any suggested mitigation. Maintainers should acknowledge
the report, reproduce it privately, prepare tests and a fix, then coordinate public
disclosure after patched artifacts are available.

## Supported configurations

Until the first alpha release, only the configurations exercised by automated tests
are considered supported. MongoDB, WebSockets, and asynchronous middleware should be
treated as experimental until their integration suites are complete.
