#pragma once

#include <string>

namespace xp::cli {

int createApp(const std::string& name);                           // create.cpp
int generate(const std::string& type, const std::string& name);   // generate.cpp
int build(bool release);                                          // build.cpp
int run(bool release);                                            // build.cpp
int watch();                                                      // dev.cpp
int doctor();                                                     // doctor.cpp
int installDeps();                                                // doctor.cpp
int clean();                                                      // clean.cpp

} // namespace xp::cli
