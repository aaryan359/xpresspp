#pragma once

#include "app.h"
#include "request.h"
#include "response.h"
#include <drogon/drogon.h>
#include <functional>
#include <string>
#include <unordered_map>
#include <cassert>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <sstream>

namespace xp {

class TestRequestBuilder {
private:
    App& app_;
    drogon::HttpRequestPtr native_req_;
    int expected_status_ = -1;
    std::unordered_map<std::string, std::string> expected_headers_;

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

    Response send(const std::string& body = "", const std::string& contentType = "application/json") {
        std::cout << "[TestClient DEBUG] Entering send(string). body size = " << body.size() << std::endl;
        try {
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
        } catch (const std::exception& e) {
            std::cerr << "[TestClient DEBUG] Error preparing request: " << e.what() << std::endl;
            throw;
        }

        std::mutex mtx;
        std::condition_variable cv;
        bool done = false;
        drogon::HttpResponsePtr response_ptr;

        std::cout << "[TestClient DEBUG] Injecting request..." << std::endl;
        try {
            app_.injectRequest(native_req_, [&](const drogon::HttpResponsePtr& res) {
                std::cout << "[TestClient DEBUG] Callback received response!" << std::endl;
                response_ptr = res;
                std::lock_guard<std::mutex> lock(mtx);
                done = true;
                cv.notify_one();
            });
        } catch (const std::exception& e) {
            std::cerr << "[TestClient DEBUG] Error during injectRequest call: " << e.what() << std::endl;
            throw;
        }

        std::cout << "[TestClient DEBUG] Waiting for condition variable..." << std::endl;
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [&] { return done; });
        std::cout << "[TestClient DEBUG] Wait finished. Constructing Response..." << std::endl;

        Response res(response_ptr);

        if (expected_status_ != -1) {
            if (res.statusCode() != expected_status_) {
                std::cerr << "[Test Failure] Expected status " << expected_status_
                          << ", but got " << res.statusCode() << "\n";
            }
            assert(res.statusCode() == expected_status_);
        }

        for (const auto& [key, value] : expected_headers_) {
            std::string actual = res.header(key);
            if (actual != value) {
                std::cerr << "[Test Failure] Expected header '" << key << "' to be '" << value
                          << "', but got '" << actual << "'\n";
            }
            assert(actual == value);
        }

        std::cout << "[TestClient DEBUG] Exiting send(string) successfully." << std::endl;
        return res;
    }

    Response send(const xp::var& jsonBody) {
        std::cout << "[TestClient DEBUG] Entering send(var)" << std::endl;
        Json::StreamWriterBuilder builder;
        std::string body;
        try {
            body = Json::writeString(builder, jsonBody);
        } catch (const std::exception& e) {
            std::cerr << "[TestClient DEBUG] Json::writeString failed: " << e.what() << std::endl;
            throw;
        }
        std::cout << "[TestClient DEBUG] Serialized body: " << body << std::endl;
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
