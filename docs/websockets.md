# WebSockets

Xpress++ wraps Drogon's highly optimized, asynchronous WebSocket server engine into an Express-like router API. This makes building real-time applications like chat rooms, live telemetry, and notifications incredibly straightforward.

---

## Basic Usage

You can declare a WebSocket route handler using `app.ws()`. The handler is called whenever a new client establishes a WebSocket handshake at that path.

```cpp
#include <xpresspp/xpresspp.h>
#include <iostream>

int main() {
    xp::App app;

    app.ws("/chat", [](xp::WebSocketConn& ws) {
        std::cout << "New connection established!" << std::endl;

        // Register message event
        ws.onMessage([&ws](const std::string& msg) {
            std::cout << "Received: " << msg << std::endl;
            ws.send("Server echo: " + msg);
        });

        // Register close event
        ws.onClose([]() {
            std::cout << "Connection closed by client." << std::endl;
        });
    });

    app.listen();
    return 0;
}
```

---

## Connection Methods

The `xp::WebSocketConn` instance passed to the connection handler exposes a clean, intuitive API:

| Method | Signature | Description |
| :--- | :--- | :--- |
| `send` | `void send(const std::string& msg)` | Sends a text message back to the client. |
| `onMessage` | `void onMessage(std::function<void(const std::string&)> cb)` | Registers a callback that runs whenever the client sends a message. |
| `onClose` | `void onClose(std::function<void()> cb)` | Registers a callback that runs when the connection is closed. |
| `close` | `void close()` | Gracefully shuts down the connection. |
| `native` | `const drogon::WebSocketConnectionPtr& native() const` | Returns the native Drogon WebSocket connection pointer for lower-level configurations. |

---

## Real-World Example: Simple Broadcast Chat

Here is a complete, thread-safe implementation of a broadcast chatroom where any message received from a client is broadcast to all other connected clients:

```cpp
#include <xpresspp/xpresspp.h>
#include <set>
#include <mutex>
#include <memory>

// Keep track of connected clients
std::set<drogon::WebSocketConnectionPtr> active_connections;
std::mutex connections_mutex;

int main() {
    xp::App app;

    app.ws("/chat", [](xp::WebSocketConn& ws) {
        // Keep a copy of the underlying connection
        auto native_conn = ws.native();

        // 1. Add connection to registry on connect
        {
            std::lock_guard<std::mutex> lock(connections_mutex);
            active_connections.insert(native_conn);
        }

        // 2. Broadcast received messages to all other clients
        ws.onMessage([native_conn](const std::string& msg) {
            std::lock_guard<std::mutex> lock(connections_mutex);
            for (const auto& conn : active_connections) {
                // Don't echo back to the sender
                if (conn != native_conn) {
                    conn->send(msg);
                }
            }
        });

        // 3. Remove connection from registry on close
        ws.onClose([native_conn]() {
            std::lock_guard<std::mutex> lock(connections_mutex);
            active_connections.erase(native_conn);
        });
    });

    app.listen();
    return 0;
}
```

---

## Lower-Level Customization

If you need advanced functionality, such as checking the peer's IP address, setting custom headers, or performing ping/pong operations, you can extract the underlying Drogon connection reference:

```cpp
app.ws("/telemetry", [](xp::WebSocketConn& ws) {
    const auto& drogon_conn = ws.native();
    
    // Get the client's IP address
    std::string client_ip = drogon_conn->peerAddr().toIp();
    
    // Check if the connection is still active
    bool active = drogon_conn->connected();
});
```
