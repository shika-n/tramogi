#pragma once

#include <expected>
#include <optional>
#include <string>

namespace tramogi::core {

template <class T = void> using Result = std::expected<T, std::string>;
template <class T> using Option = std::optional<T>;

} // namespace tramogi::core
