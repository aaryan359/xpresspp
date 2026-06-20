#pragma once

#include <drogon/WebSocketController.h>
#include <string>
#include <functional>
#include <unordered_map>
#include <memory>

namespace xp {

class WebSocketConn {
private:
    drogon::WebSocketConnectionPtr conn_;
    std::function<void(const std::string&)> on_message_;
    std::function<void()> on_close_;

public:
    explicit WebSocketConn(drogon::WebSocketConnectionPtr conn) : conn_(std::move(conn)) {}

    void send(const std::string& msg) {
        conn_->send(msg);
    }

    void onMessage(std::function<void(const std::string&)> cb) {
        on_message_ = std::move(cb);
    }

    void onClose(std::function<void()> cb) {
        on_close_ = std::move(cb);
    }

    void close() {
        conn_->shutdown();
    }

    void triggerMessage(std::string&& msg) {
        if (on_message_) {
            on_message_(msg);
        }
    }

    void triggerClose() {
        if (on_close_) {
            on_close_();
        }
    }
    
    const drogon::WebSocketConnectionPtr& native() const {
        return conn_;
    }
};

struct WebSocketRegistry {
    static std::unordered_map<std::string, std::function<void(WebSocketConn&)>>& routes() {
        static std::unordered_map<std::string, std::function<void(WebSocketConn&)>> instance;
        return instance;
    }
};

class XpressppWebSocketController : public drogon::WebSocketController<XpressppWebSocketController> {
public:
    void handleNewConnection(const drogon::HttpRequestPtr& req, const drogon::WebSocketConnectionPtr& conn) override {
        std::string path = req->getPath();
        auto& routes = WebSocketRegistry::routes();
        auto it = routes.find(path);
        if (it != routes.end()) {
            auto ws_conn = std::make_shared<WebSocketConn>(conn);
            conn->setContext(ws_conn);
            it->second(*ws_conn);
        } else {
            conn->shutdown();
        }
    }

    void handleNewMessage(const drogon::WebSocketConnectionPtr& conn, std::string&& message, const drogon::WebSocketMessageType& /*type*/) override {
        if (!conn->hasContext()) return;
        try {
            auto ws_conn = conn->getContext<WebSocketConn>();
            if (ws_conn) {
                ws_conn->triggerMessage(std::move(message));
            }
        } catch (...) {}
    }

    void handleConnectionClosed(const drogon::WebSocketConnectionPtr& conn) override {
        if (!conn->hasContext()) return;
        try {
            auto ws_conn = conn->getContext<WebSocketConn>();
            if (ws_conn) {
                ws_conn->triggerClose();
            }
        } catch (...) {}
    }

    WS_PATH_LIST_BEGIN
    WS_ADD_PATH_VIA_REGEX("/.*", drogon::Get);
    WS_PATH_LIST_END
};

} // namespace xp
