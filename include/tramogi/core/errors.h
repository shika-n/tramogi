#pragma once

#include <expected>
#include <optional>
#include <string>

namespace tramogi::core {

template <class T = void> using Result = std::expected<T, std::string>;
template <class T> using Option = std::optional<T>;
using Error = std::unexpected<std::string>;

} // namespace tramogi::core
