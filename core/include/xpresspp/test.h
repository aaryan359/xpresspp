#pragma once

#include "app.h"
#include "request.h"
#include "response.h"
#include <drogon/drogon.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <sstream>
#include <chrono>
#include <stdexcept>

namespace xp {

class TestRequestBuilder {
private:
    App& app_;
    drogon::HttpRequestPtr native_req_;
    int expected_status_ = -1;
    std::unordered_map<std::string, std::string> expected_headers_;
    std::chrono::milliseconds timeout_{5000};

public:
    TestRequestBuilder(App& app, const std::string& method, const std::string& path)
        : app_(app), native_req_(drogon::HttpRequest::newHttpRequest()) {
        auto q_pos = path.find('?');
        if (q_pos != std::string::npos) {
            std::string actual_path = path.substr(0, q_pos);
            std::string query_str = path.substr(q_pos + 1);
            native_req_->setPath(actual_path);
            
            std::stringstream ss(query_str);
            std::string pair;
            while (std::getline(ss, pair, '&')) {
                auto eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = pair.substr(0, eq_pos);
                    std::string val = pair.substr(eq_pos + 1);
                    native_req_->setParameter(key, val);
                } else {
                    native_req_->setParameter(pair, "");
                }
            }
        } else {
            native_req_->setPath(path);
        }

        drogon::HttpMethod m = drogon::Get;
        if (method == "POST") m = drogon::Post;
        else if (method == "PUT") m = drogon::Put;
        else if (method == "DELETE") m = drogon::Delete;
        else if (method == "PATCH") m = drogon::Patch;
        else if (method == "OPTIONS") m = drogon::Options;
        else if (method == "HEAD") m = drogon::Head;
        native_req_->setMethod(m);
    }

    TestRequestBuilder& set(const std::string& key, const std::string& value) {
        native_req_->addHeader(key, value);
        return *this;
    }

    TestRequestBuilder& expectStatus(int status) {
        expected_status_ = status;
        return *this;
    }

    TestRequestBuilder& expectHeader(const std::string& key, const std::string& value) {
        expected_headers_[key] = value;
        return *this;
    }

    TestRequestBuilder& timeout(std::chrono::milliseconds value) {
        timeout_ = value;
        return *this;
    }

    Response send(const std::string& body = "", const std::string& contentType = "application/json") {
        if (!body.empty()) {
            native_req_->setBody(body);
            native_req_->addHeader("content-type", contentType);
            if (contentType == "application/json") {
                native_req_->setContentTypeCode(drogon::CT_APPLICATION_JSON);
            } else if (contentType == "text/plain") {
                native_req_->setContentTypeCode(drogon::CT_TEXT_PLAIN);
            } else if (contentType == "text/html") {
                native_req_->setContentTypeCode(drogon::CT_TEXT_HTML);
            }
        }

        std::mutex mtx;
        std::condition_variable cv;
        bool done = false;
        drogon::HttpResponsePtr response_ptr;

        app_.injectRequest(native_req_, [&](const drogon::HttpResponsePtr& res) {
            response_ptr = res;
            std::lock_guard<std::mutex> lock(mtx);
            done = true;
            cv.notify_one();
        });

        std::unique_lock<std::mutex> lock(mtx);
        if (!cv.wait_for(lock, timeout_, [&] { return done; })) {
            throw std::runtime_error("Xpress++ test request timed out after " +
                                     std::to_string(timeout_.count()) + "ms");
        }

        Response res(response_ptr);

        if (expected_status_ != -1) {
            if (res.statusCode() != expected_status_) {
                throw std::runtime_error("Expected status " + std::to_string(expected_status_) +
                                         ", got " + std::to_string(res.statusCode()));
            }
        }

        for (const auto& [key, value] : expected_headers_) {
            std::string actual = res.header(key);
            if (actual != value) {
                throw std::runtime_error("Expected header '" + key + "' to be '" + value +
                                         "', got '" + actual + "'");
            }
        }

        return res;
    }

    Response send(const xp::var& jsonBody) {
        Json::StreamWriterBuilder builder;
        const auto body = Json::writeString(builder, jsonBody);
        return send(body, "application/json");
    }
};

class TestClient {
private:
    App& app_;

public:
    explicit TestClient(App& app) : app_(app) {}

    TestRequestBuilder get(const std::string& path) { return TestRequestBuilder(app_, "GET", path); }
    TestRequestBuilder post(const std::string& path) { return TestRequestBuilder(app_, "POST", path); }
    TestRequestBuilder put(const std::string& path) { return TestRequestBuilder(app_, "PUT", path); }
    TestRequestBuilder del(const std::string& path) { return TestRequestBuilder(app_, "DELETE", path); }
    TestRequestBuilder patch(const std::string& path) { return TestRequestBuilder(app_, "PATCH", path); }
};

inline TestClient request(App& app) {
    return TestClient(app);
}

} // namespace xp
