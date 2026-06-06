#include <xpresspp/xpresspp.h>
#include <stdexcept>
#include <iostream>

using namespace std;

int main() {
    xp::App app;

    
    app.get("/api/ping", [](xp::Request& req, xp::Response& res) {


        auto data = xp::array({1,1,1,1,1,1,1,1,1,1,1,1,1,1,1});

        res.json({
            {"status",    "healthy"},
            {"data", data}
        });
    });

    app.post("/api/greet", [](xp::Request& req, xp::Response& res) {
        auto body = req.json();

        cout<<" name is not coming "<<endl;

        string name = body["name"].asString();
        auto num = xp::number(10);
        auto obj = xp::object({"key", "value"});
        auto nested = xp::object({
            {"nested_key", xp::object({"inner_key", "inner_value"})},
            {"nested_num", xp::number(42)}
        });
        auto arr = xp::array({1,2,3,4,5});

        if(name.empty()){
            throw xp::BadRequestError("Name is required");
        }
        res.json({
            {"message", "Hello " + name}
        });
    });

    app.listen(8082);
    return 0;
}
