#pragma once

#include <string>
#include "common.h"

namespace xp::cli {

int createApp(const std::string& name);
int build(bool release);
int run(bool release);
int watch();
int doctor();
int installDeps();
int clean();
int migrate();

} // namespace xp::cli
