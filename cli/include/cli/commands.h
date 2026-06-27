#pragma once

#include <string>
#include "common.h"

namespace xp::cli {

int createApp(const std::string& name);
int generate(const std::string& type, const std::string& name);
int build(bool release);
int run(bool release);
int watch();
int doctor();
int clean();
int installDeps();
int migrate(const std::string& arg1 = "", const std::string& arg2 = "");
int dockerize();

} // namespace xp::cli
