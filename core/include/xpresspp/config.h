#pragma once

#include <cstdlib>
#include <string>

namespace xp {

class Config {
public:
    static std::string env(const std::string& key, const std::string& fallback = "") {
        const char* value = std::getenv(key.c_str());
        return value == nullptr ? fallback : std::string(value);
    }

    static int envInt(const std::string& key, int fallback) {
        const auto value = env(key);
        if (value.empty()) {
            return fallback;
        }
        try {
            return std::stoi(value);
        } catch (...) {
            return fallback;
        }
    }

    static bool envBool(const std::string& key, bool fallback = false) {
        auto value = env(key);
        for (auto& c : value) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (value == "1" || value == "true" || value == "yes" || value == "on") {
            return true;
        }
        if (value == "0" || value == "false" || value == "no" || value == "off") {
            return false;
        }
        return fallback;
    }
};

} // namespace xp
