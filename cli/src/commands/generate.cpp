#include "commands/commands.h"
#include "util/util.h"

#include <cctype>
#include <fstream>
#include <iostream>
#include <vector>

namespace xp::cli {

// "user-profile" / "User Profile" / "user_profile"  →  ["user", "profile"]
static std::vector<std::string> splitWords(const std::string& name) {
    std::vector<std::string> words;
    std::string current;

    for (char c : name) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            current += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        } else if (!current.empty()) {
            words.push_back(current);
            current.clear();
        }
    }
    if (!current.empty()) words.push_back(current);
    return words;
}

static std::string snakeCase(const std::vector<std::string>& words) {
    std::string result;
    for (const auto& word : words) result += (result.empty() ? "" : "_") + word;
    return result;
}

static std::string pascalCase(const std::vector<std::string>& words) {
    std::string result;
    for (auto word : words) {
        word[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(word[0])));
        result += word;
    }
    return result;
}

// A kind of file `xp generate` can make.
// In the template, @SNAKE@ is user_profile and @PASCAL@ is UserProfile.
struct Generator {
    const char* type;
    const char* folder;
    const char* fileSuffix;  // added after the snake_case name
    const char* tip;         // printed after the file is made
    const char* content;
};

static const std::vector<Generator> generators = {
    {"route", "src/routes", ".h",
     "Mount it with: app.use(\"/@SNAKE@\", @SNAKE@Routes());",
     R"(#pragma once
#include <xpresspp/xpresspp.h>

inline xp::Router @SNAKE@Routes() {
    xp::Router router;

    router.get("/", [](xp::Request& req, xp::Response& res) {
        res.ok({{"resource", "@SNAKE@"}});
    });

    router.post("/", [](xp::Request& req, xp::Response& res) {
        res.created({{"created", true}, {"data", req.json()}});
    });

    return router;
}
)"},

    {"controller", "src/controllers", "_controller.h", "",
     R"(#pragma once
#include <xpresspp/xpresspp.h>

struct @PASCAL@Controller {
    static void index(xp::Request& req, xp::Response& res) {
        res.ok({{"resource", "@SNAKE@"}});
    }

    static void show(xp::Request& req, xp::Response& res) {
        res.ok({{"id", req.param("id", true)}});
    }
};
)"},

    {"middleware", "src/middleware", ".h",
     "Use it with: app.use(@SNAKE@());",
     R"(#pragma once
#include <xpresspp/xpresspp.h>

inline xp::Middleware @SNAKE@() {
    return [](xp::Request& req, xp::Response& res, xp::Next next) {
        next();
    };
}
)"},

    {"model", "src/models", ".h", "",
     R"(#pragma once
#include <json/json.h>
#include <string>

struct @PASCAL@ {
    int         id = 0;
    std::string name;

    Json::Value toJson() const {
        Json::Value value;
        value["id"]   = id;
        value["name"] = name;
        return value;
    }
};
)"},
};



static std::string replaceAll(std::string text, const std::string& from, const std::string& to) {
    for (auto at = text.find(from); at != std::string::npos; at = text.find(from, at + to.size())) {
        text.replace(at, from.size(), to);
    }
    return text;
}




int generate(const std::string& type, const std::string& name) {
    if (!requireProject()) return 1;

    const Generator* generator = nullptr;
    for (const auto& candidate : generators) {
        if (type == candidate.type || type == std::string(candidate.type) + "s") generator = &candidate;
    }
    if (!generator) {
        error("Unknown type: \"" + type + "\"", "Choose one of: route, controller, middleware, model.",
              "xp generate route users");
        return 1;
    }

    const auto words = splitWords(name);
    if (words.empty()) {
        error("\"" + name + "\" is not a usable name.", "Use letters and numbers.", "xp generate route users");
        return 1;
    }
    const std::string snake  = snakeCase(words);
    const std::string pascal = pascalCase(words);

    const fs::path path = fs::path(generator->folder) / (snake + generator->fileSuffix);
    if (fs::exists(path)) {
        error("File already exists: " + path.string(), "", "Pick another name or delete the file.");
        return 1;
    }

    auto fill = [&](std::string text) {
        return replaceAll(replaceAll(std::move(text), "@SNAKE@", snake), "@PASCAL@", pascal);
    };

    fs::create_directories(path.parent_path());
    std::ofstream(path) << fill(generator->content);

    success("Created " + path.string());
    if (*generator->tip) std::cout << colour::dim() << "  " << fill(generator->tip) << colour::reset() << "\n";
    return 0;
}

} // namespace xp::cli
