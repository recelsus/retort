#include "env.h"

#include <cstdlib>

namespace retort
{
std::optional<std::string> get_env(std::string_view name) {
    const auto *value = std::getenv(std::string{name}.c_str());
    if (value == nullptr) {
        return std::nullopt;
    }
    return std::string{value};
}

std::string get_env_or(std::string_view name, const std::string &fallback) {
    const auto present = get_env(name);
    if (!present.has_value()) {
        return fallback;
    }
    return *present;
}
}
