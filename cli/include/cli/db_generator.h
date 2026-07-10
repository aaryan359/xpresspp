#pragma once

#include "schema_ir.h"

#include <string>

namespace xp::cli {

void generateDatabaseHeader(const SchemaIR& schema, const std::string& path);

} // namespace xp::cli
