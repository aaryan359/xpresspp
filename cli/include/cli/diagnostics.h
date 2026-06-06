#pragma once

#include <string>
#include <vector>
#include "common.h"

namespace xp::cli {

struct Diagnostic {
    std::string filePath;
    std::string lineNum;
    std::string colNum;
    std::string severity;
    std::string message;
    std::vector<std::string> snippet;
};

int runBuildAndFilter(const std::string& command, std::string& fullOutput);
void explainCompilerErrors(const std::string& raw_output);

} // namespace xp::cli
