#pragma once

#include <cstdint>
#include <expected>
#include <optional>
#include <string>

namespace tramogi::core {

template <class T = void> using Result = std::expected<T, std::string>;
using Error = std::unexpected<std::string>;

template <class T> using Optional = std::optional<T>;
namespace optional {
constexpr std::nullopt_t none = std::nullopt;
}

template <class T> struct Vec2 {
	T x;
	T y;
};
using Size = Vec2<int32_t>;
using Extent = Vec2<uint32_t>;

} // namespace tramogi::core
