#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace retort
{
std::optional<std::string> get_env(std::string_view name);
std::string get_env_or(std::string_view name, const std::string &fallback);
}
