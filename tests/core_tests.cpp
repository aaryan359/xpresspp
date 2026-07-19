#include <xpresspp/xpresspp.h>

#include <iostream>
#include <fstream>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string cookieValue(const std::string& set_cookie) {
    const auto equals = set_cookie.find('=');
    const auto end = set_cookie.find(';', equals + 1);
    if (equals == std::string::npos) return {};
    return set_cookie.substr(equals + 1, end - equals - 1);
}

void testRoutingAndErrors() {
    xp::App app;
    app.debug(false);
    app.get("/users/:id", [](xp::Request& req, xp::Response& res) {
        res.ok({{"id", req.param("id")}});
    });
    app.get("/explode", [](xp::Request&, xp::Response&) {
        throw std::runtime_error("secret internal detail");
    });

    const auto user = xp::request(app).get("/users/42").expectStatus(200).send();
    require(user.json()["id"].asString() == "42", "route parameter was not captured");

    const auto error = xp::request(app).get("/explode").expectStatus(500).send();
    require(error.json()["message"].asString() == "Internal server error",
            "production errors must be redacted");
}

void testJwt() {
    Json::Value claims;
    claims["sub"] = "user-1";
    claims["iss"] = "xpresspp-tests";
    claims["aud"] = "xpresspp-app";
    const auto token = xp::generateJwt(claims, "a-long-test-secret", 60);

    xp::JwtVerifyOptions options;
    options.issuer = "xpresspp-tests";
    options.audience = "xpresspp-app";
    std::string subject;
    require(xp::verifyJwt(token, "a-long-test-secret", subject, options), "valid JWT rejected");
    require(subject == "user-1", "JWT subject not returned");

    auto tampered = token;
    tampered.back() = tampered.back() == 'a' ? 'b' : 'a';
    require(!xp::verifyJwt(tampered, "a-long-test-secret", subject, options),
            "tampered JWT accepted");
}

void testSessionAndCsrf() {
    auto session = xp::session();
    auto make_session = [&]() {
        auto native = drogon::HttpRequest::newHttpRequest();
        native->setPath("/");
        xp::Request req(native);
        xp::Response res;
        bool next_called = false;
        session(req, res, [&] { next_called = true; });
        require(next_called, "session middleware did not continue");
        require(req.local<std::string>("sessionId").size() == 64, "session ID is not 256-bit hex");
        return cookieValue(res.header("set-cookie"));
    };
    require(make_session() != make_session(), "session IDs must be unpredictable and unique");

    auto csrf = xp::csrf();
    auto get_native = drogon::HttpRequest::newHttpRequest();
    get_native->setMethod(drogon::Get);
    xp::Request get_req(get_native);
    xp::Response get_res;
    bool get_next = false;
    csrf(get_req, get_res, [&] { get_next = true; });
    const auto token = get_res.header("x-csrf-token");
    require(get_next && token.size() == 64, "CSRF token was not issued");

    auto post_native = drogon::HttpRequest::newHttpRequest();
    post_native->setMethod(drogon::Post);
    post_native->addCookie("csrf_token", token);
    post_native->addHeader("x-csrf-token", token);
    xp::Request post_req(post_native);
    xp::Response post_res;
    bool post_next = false;
    csrf(post_req, post_res, [&] { post_next = true; });
    require(post_next, "valid CSRF token rejected");
}

void testRateLimit() {
    xp::RateLimitOptions options;
    options.max = 1;
    options.keyGenerator = [](xp::Request&) { return "test-client"; };
    auto limiter = xp::rateLimit(options);

    auto run = [&](int expected_status) {
        xp::Request req(drogon::HttpRequest::newHttpRequest());
        xp::Response res;
        bool next_called = false;
        limiter(req, res, [&] { next_called = true; });
        if (expected_status == 200) require(next_called, "first request was limited");
        else require(!next_called && res.statusCode() == expected_status, "rate limit was not enforced");
    };
    run(200);
    run(429);
}

void testMemoryCache() {
    xp::MemoryCache cache;
    cache.set("answer", 42);
    require(cache.has("answer"), "cache did not retain value");
    require(cache.getOr("answer").asInt() == 42, "cache returned wrong value");
    require(cache.remove("answer"), "cache remove failed");
    require(!cache.has("answer"), "cache retained removed value");
}

void testAsyncMiddlewareContract() {
    xp::Request req(drogon::HttpRequest::newHttpRequest());
    xp::Response res;
    bool next_called = false;
    xp::AsyncMiddleware middleware = [](xp::Request& request,
                                        xp::Response&,
                                        xp::AsyncNext next) -> xp::Task<void> {
        request.locals["before"] = true;
        co_await next();
        request.locals["after"] = true;
    };
    drogon::sync_wait(middleware(req, res, [&]() -> xp::Task<void> {
        next_called = true;
        co_return;
    }));
    require(next_called, "async middleware did not await next");
    require(req.local<bool>("before") && req.local<bool>("after"),
            "async middleware did not resume after next");
}

void testStaticTraversalProtection() {
    const auto base = std::filesystem::temp_directory_path() / "xpresspp-static-test";
    const auto public_dir = base / "public";
    std::filesystem::create_directories(public_dir);
    { std::ofstream(public_dir / "index.html") << "public"; }
    { std::ofstream(base / "secret.txt") << "secret"; }

    xp::App app;
    app.staticFiles(public_dir, "/assets");
    const auto public_response = xp::request(app).get("/assets/index.html").expectStatus(200).send();
    require(public_response.body() == "public", "valid static file was not served");
    const auto escaped = xp::request(app).get("/assets/../secret.txt").expectStatus(404).send();
    require(escaped.body() != "secret", "static mount allowed path traversal");
    std::filesystem::remove_all(base);
}

struct QueryTestModel {};

static_assert(!std::is_copy_constructible_v<xp::data::TransactionContext>);
static_assert(!std::is_move_constructible_v<xp::data::TransactionContext>);

void testTypedQueryRendering() {
    xp::data::Column<QueryTestModel, std::int64_t> id(0);
    xp::data::Column<QueryTestModel, std::string> name(1);
    xp::data::QuerySpec<QueryTestModel> spec;
    spec.where = (id >= 10) && name.startsWith("ary");
    spec.orderBy.push_back(name.asc());
    spec.limit = 25;
    spec.offset = 5;

    xp::data::PostgreSqlRenderer<QueryTestModel> renderer(
        [](std::size_t field) -> std::string_view {
            static constexpr std::string_view fields[] = {"id", "name"};
            return field < std::size(fields) ? fields[field] : std::string_view{};
        });
    const auto query = renderer.select("users", "id, name", spec);
    require(query.sql ==
        "SELECT id, name FROM users WHERE (id >= $1 AND name LIKE $2) ORDER BY name ASC LIMIT 25 OFFSET 5",
        "typed PostgreSQL query rendered incorrectly: " + query.sql);
    require(query.parameters.size() == 2, "typed query did not bind both values");

    bool rejected = false;
    try {
        renderer.remove("users", {});
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    require(rejected, "delete without where was not rejected");

    xp::data::Patch<std::optional<std::string>> patch;
    require(!patch.present(), "empty patch was marked present");
    patch = nullptr;
    require(patch.present() && !patch.value(), "explicit SQL NULL was not preserved");
}

void testAiChatHelper() {
    xp::App app;
    app.post("/chat", xp::ai::chat([](const xp::ai::ChatRequest& input) {
        xp::ai::ChatResponse output;
        output.reply = "hello " + input.message;
        output.model = input.model;
        output.metadata["tokens"] = 2;
        return output;
    }));

    xp::var body;
    body["message"] = "native ai";
    body["model"] = "test-model";
    const auto response = xp::request(app).post("/chat").expectStatus(200).send(body);
    require(response.json()["reply"].asString() == "hello native ai",
            "AI chat helper did not return generated reply");
    require(response.json()["model"].asString() == "test-model",
            "AI chat helper did not preserve model name");
    require(response.json()["metadata"]["tokens"].asInt() == 2,
            "AI chat helper did not preserve metadata");
    require(response.json().isMember("durationMs"),
            "AI chat helper did not include duration");

    xp::var invalid;
    invalid["prompt"] = "";
    xp::request(app).post("/chat").expectStatus(400).send(invalid);
}

} // namespace

int main() {
    try {
        testRoutingAndErrors();
        testJwt();
        testSessionAndCsrf();
        testRateLimit();
        testMemoryCache();
        testAsyncMiddlewareContract();
        testStaticTraversalProtection();
        testTypedQueryRendering();
        testAiChatHelper();
        std::cout << "Xpress++ core tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Xpress++ core tests failed: " << error.what() << "\n";
        return 1;
    }
}
