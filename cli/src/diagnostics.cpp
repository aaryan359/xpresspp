#include "cli/diagnostics.h"
#include <iostream>
#include <sstream>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace xp::cli {

int runBuildAndFilter(const std::string& command, std::string& fullOutput) {
#if defined(_WIN32)
    // Fallback for Windows
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return -1;
    char buf[512];
    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        fullOutput += buf;
        std::cout << buf;
    }
    int status = pclose(pipe);
    return status;
#else
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return -1;

    char buf[512];
    bool printCurrentLine = false;

    while (fgets(buf, sizeof(buf), pipe) != nullptr) {
        std::string line(buf);
        fullOutput += line;

        // Strip trailing newline for checks
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
            line.pop_back();
        }

        bool isError = (line.find(": error:") != std::string::npos || line.find(": fatal error:") != std::string::npos);
        bool isWarning = (line.find(": warning:") != std::string::npos);
        bool isProgress = (line.find("[ ") == 0 || line.find("Building CXX") != std::string::npos || line.find("Linking CXX") != std::string::npos);
        bool hasPipe = (line.find('|') != std::string::npos);

        if (isProgress) {
            std::cout << colour::cyan() << line << colour::reset() << "\n";
            printCurrentLine = false;
        } else if (isError || isWarning) {
            std::cout << (isError ? colour::red() : colour::yellow()) << line << colour::reset() << "\n";
            printCurrentLine = true;
        } else if (printCurrentLine && hasPipe) {
            std::cout << line << "\n";
        } else {
            printCurrentLine = false;
        }
    }

    int status = pclose(pipe);
    return WEXITSTATUS(status);
#endif
}

void explainCompilerErrors(const std::string& raw_output) {
    std::vector<Diagnostic> diagnostics;
    std::istringstream stream(raw_output);
    std::string line;
    Diagnostic currentDiag;
    bool collectingSnippet = false;

    while (std::getline(stream, line)) {
        size_t errPos = line.find(": error:");
        size_t fatalPos = line.find(": fatal error:");
        size_t warnPos = line.find(": warning:");
        
        bool isDiagStart = (errPos != std::string::npos || fatalPos != std::string::npos || warnPos != std::string::npos);

        if (isDiagStart) {
            if (!currentDiag.message.empty()) {
                diagnostics.push_back(currentDiag);
                currentDiag = Diagnostic();
            }
            
            size_t markerPos = std::string::npos;
            if (errPos != std::string::npos) {
                currentDiag.severity = "error";
                markerPos = errPos;
            } else if (fatalPos != std::string::npos) {
                currentDiag.severity = "fatal error";
                markerPos = fatalPos;
            } else {
                currentDiag.severity = "warning";
                markerPos = warnPos;
            }

            size_t msgStart = line.find(':', markerPos + 2);
            if (msgStart != std::string::npos) {
                currentDiag.message = line.substr(msgStart + 2);
            } else {
                currentDiag.message = line.substr(markerPos);
            }

            std::string prefix = line.substr(0, markerPos);
            size_t lastColon = prefix.rfind(':');
            if (lastColon != std::string::npos) {
                std::string colStr = prefix.substr(lastColon + 1);
                size_t prevColon = prefix.rfind(':', lastColon - 1);
                if (prevColon != std::string::npos) {
                    currentDiag.lineNum = prefix.substr(prevColon + 1, lastColon - prevColon - 1);
                    currentDiag.filePath = prefix.substr(0, prevColon);
                    currentDiag.colNum = colStr;
                } else {
                    currentDiag.lineNum = colStr;
                    currentDiag.filePath = prefix;
                }
            } else {
                currentDiag.filePath = prefix;
            }
            
            collectingSnippet = true;
        } else if (collectingSnippet) {
            // GCC/Clang source line numbers and code printout have a '|'
            if (line.find('|') != std::string::npos) {
                currentDiag.snippet.push_back(line);
            } else {
                collectingSnippet = false;
            }
        }
    }

    if (!currentDiag.message.empty()) {
        diagnostics.push_back(currentDiag);
    }

    if (diagnostics.empty()) {
        return;
    }

    std::cerr << "\n" << colour::red() << colour::bold() << "  ▸ Friendly Build Diagnostics Summary:" << colour::reset() << "\n";
    divider();

    for (const auto& diag : diagnostics) {
        std::string displayPath = diag.filePath;
        try {
            if (displayPath.rfind(fs::current_path().string(), 0) == 0) {
                displayPath = displayPath.substr(fs::current_path().string().length() + 1);
            }
        } catch (...) {}

        std::cerr << "  " << colour::bold() << displayPath << ":" << diag.lineNum << ":" << diag.colNum << colour::reset() << " - ";
        if (diag.severity == "warning") {
            std::cerr << colour::yellow() << colour::bold() << "warning: " << colour::reset();
        } else {
            std::cerr << colour::red() << colour::bold() << "error: " << colour::reset();
        }
        std::cerr << diag.message << "\n\n";

        for (const auto& sLine : diag.snippet) {
            std::cerr << "    " << sLine << "\n";
        }
        std::cerr << "\n";

        std::string hint = "";
        
        if (diag.message.find("operator+") != std::string::npos && 
            (diag.message.find("Json::Value") != std::string::npos || diag.message.find("Value") != std::string::npos)) {
            hint = "You are concatenating a string literal and a Json::Value directly (e.g. \"Hello \" + name).\n"
                   "          In C++, you must convert the JSON value to a string first:\n"
                   "          Change: \"Hello \" + name\n"
                   "          To:     \"Hello \" + name.asString()";
        }
        else if (diag.message.find("nullptr_t") != std::string::npos && 
                 (diag.message.find("Json::Value") != std::string::npos || diag.message.find("Value") != std::string::npos)) {
            hint = "You are trying to pass 'nullptr' as a JSON null value.\n"
                   "          In C++, 'nullptr' cannot be implicitly converted to a JSON null.\n"
                   "          Change: nullptr\n"
                   "          To:     xp::null()";
        }
        else if (diag.message.find("xp::Response::json") != std::string::npos || 
                 (diag.message.find("json") != std::string::npos && diag.message.find("initializer_list") != std::string::npos)) {
            hint = "No matching function to pass this initializer list to res.json().\n"
                   "          Make sure all keys are std::string / const char* and all values are standard types (string, int, bool, double).\n"
                   "          - If you are trying to pass a list or vector (e.g. {1, 2, 3}), wrap it with xp::array():\n"
                   "            Change: {\"data\", data}\n"
                   "            To:     {\"data\", xp::array(data)}\n"
                   "          - If you used a Json::Value in string concatenation inside this list, call .asString() first.";
        }
        else if (diag.message.find("operator[]") != std::string::npos && 
                 (diag.message.find("xp::Request") != std::string::npos || diag.message.find("Request") != std::string::npos)) {
            hint = "You cannot use bracket notation directly on the Request object.\n"
                   "          Use the request methods instead:\n"
                   "          - URL parameters:   req.param(\"...\")\n"
                   "          - Query parameters: req.query(\"...\")\n"
                   "          - Parsed JSON body: req.json()[\"...\"]";
        }
        else if (diag.message.find("expected ';' before") != std::string::npos) {
            hint = "You missed a semicolon (;) at the end of the previous line.";
        }
        else if (diag.message.find("was not declared in this scope") != std::string::npos) {
            size_t qStart = diag.message.find("‘");
            size_t qEnd = diag.message.find("’");
            if (qStart == std::string::npos || qEnd == std::string::npos) {
                qStart = diag.message.find('\'');
                qEnd = diag.message.rfind('\'');
            }
            std::string varName = "";
            if (qStart != std::string::npos && qEnd != std::string::npos && qEnd > qStart) {
                // Adjust for size of multibyte UTF-8 characters if we matched "‘" and "’"
                size_t offset = (diag.message[qStart] != '\'') ? 3 : 1;
                varName = diag.message.substr(qStart + offset, qEnd - qStart - offset);
            }
            if (!varName.empty()) {
                hint = "The variable '" + varName + "' is not defined in this scope.\n"
                       "          Check if you misspelled it or forgot to declare it (e.g., auto " + varName + " = ...).";
            } else {
                hint = "A variable is used before it is declared or is out of scope.";
            }
        }
        else if ((diag.message.find("no matching function for call to") != std::string::npos || diag.message.find("no match for call to") != std::string::npos) &&
                 (diag.message.find("get") != std::string::npos || diag.message.find("post") != std::string::npos || diag.message.find("use") != std::string::npos)) {
            hint = "The route/middleware handler signature is incorrect.\n"
                   "          Standard signatures:\n"
                   "          - Route handler: [](xp::Request& req, xp::Response& res) { ... }\n"
                   "          - Middleware:    [](xp::Request& req, xp::Response& res, xp::Next next) { ... }";
        }

        if (!hint.empty()) {
            std::cerr << "  " << colour::yellow() << colour::bold() << "▸ Friendly Hint: " << colour::reset()
                      << colour::yellow() << hint << colour::reset() << "\n\n";
        }
        divider();
    }
}

} // namespace xp::cli
