#include "util/util.h"

#include <cstdio>
#include <iostream>
#include <regex>
#include <sstream>
#include <sys/wait.h>
#include <vector>

namespace xp::cli {

static bool contains(const std::string& text, const char* part) {
    return text.find(part) != std::string::npos;
}

// ── Build output ─────────────────────────────────────────────────────────────

int runBuild(const std::string& command, std::string& output) {
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return -1;

    bool showingSource = false;  // true while printing the code lines under an error
    char buffer[1024];

    while (fgets(buffer, sizeof(buffer), pipe)) {
        std::string line = buffer;
        output += line;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();

        const bool isError   = contains(line, ": error:") || contains(line, ": fatal error:");
        const bool isWarning = contains(line, ": warning:");
        const bool isStep    = contains(line, "Building CXX") || contains(line, "Linking CXX");

        if (isStep) {
            std::cout << colour::cyan() << line << colour::reset() << "\n";
            showingSource = false;
        } else if (isError || isWarning) {
            std::cout << (isError ? colour::red() : colour::yellow()) << line << colour::reset() << "\n";
            showingSource = true;
        } else if (showingSource && contains(line, "|")) {
            std::cout << line << "\n";
        } else {
            showingSource = false;
        }
    }
    return WEXITSTATUS(pclose(pipe));
}

// ── Friendly explanations ────────────────────────────────────────────────────

// Add a new entry here to teach `xp` about another common mistake.
// A hint is shown when the compiler message contains ALL of the `keywords`.
struct Hint {
    std::vector<const char*> keywords;
    const char* text;
};

static const std::vector<Hint> hints = {
    {{"operator+", "Value"},
     "You are adding a string and a Json::Value.\n"
     "          Convert it first:  \"Hello \" + name.asString()"},

    {{"nullptr_t", "Value"},
     "nullptr cannot be used as a JSON null.\n"
     "          Use xp::null() instead."},

    {{"operator[]", "Request"},
     "You cannot index the Request object directly. Use:\n"
     "          req.param(\"id\")   req.query(\"q\")   req.json()[\"field\"]"},

    {{"expected ';'"},
     "A semicolon (;) is missing at the end of the previous line."},

    {{"no matching function", "get"},
     "The handler has the wrong signature. It should be:\n"
     "          [](xp::Request& req, xp::Response& res) { ... }"},

    {{"no matching function", "use"},
     "Middleware has the wrong signature. It should be:\n"
     "          [](xp::Request& req, xp::Response& res, xp::Next next) { ... }"},

    {{"was not declared"},
     "A name is used before it is declared. Check the spelling,\n"
     "          and that you included <xpresspp/xpresspp.h>."},
};

static const char* findHint(const std::string& message) {
    for (const auto& hint : hints) {
        bool all = true;
        for (const char* keyword : hint.keywords) all = all && contains(message, keyword);
        if (all) return hint.text;
    }
    return nullptr;
}

void explainErrors(const std::string& compilerOutput) {
    // Matches:  path/file.cpp:12:5: error: message
    static const std::regex pattern(R"(^(.+?):(\d+):(\d+): (fatal error|error|warning): (.*)$)");

    std::istringstream lines(compilerOutput);
    std::string line;
    bool printedHeader = false;

    while (std::getline(lines, line)) {
        std::smatch match;
        if (!std::regex_match(line, match, pattern)) continue;

        const bool isWarning = match[4] == "warning";
        const std::string message = match[5];

        if (!printedHeader) {
            std::cerr << "\n" << colour::red() << colour::bold() << "  Build problems:" << colour::reset() << "\n";
            divider();
            printedHeader = true;
        }

        fs::path file = match[1].str();
        std::error_code ignored;
        const fs::path relative = fs::relative(file, fs::current_path(), ignored);
        if (!ignored && !relative.empty() && *relative.begin() != "..") file = relative;

        std::cerr << "  " << colour::bold() << file.string() << ":" << match[2] << colour::reset() << "  "
                  << (isWarning ? colour::yellow() : colour::red()) << match[4] << colour::reset()
                  << "  " << message << "\n";

        if (!isWarning) {
            if (const char* hint = findHint(message)) {
                std::cerr << "  " << colour::yellow() << "  ▸ " << hint << colour::reset() << "\n";
            }
        }
        std::cerr << "\n";
    }
}

} // namespace xp::cli
