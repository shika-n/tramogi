#pragma once

#include <format>
#include <string_view>

namespace tramogi::core::logging {

#ifndef NDEBUG
constexpr bool enable_debug_log = true;
#else
constexpr bool enable_debug_log = false;
#endif

namespace intern {
void log_impl(std::string_view format, std::format_args args);
}

template <typename... Args> void debug_log(std::format_string<Args...> format, Args &&...args) {
	if constexpr (enable_debug_log) {
		intern::log_impl(format.get(), std::make_format_args(args...));
	}
}

template <typename... Args> void log(std::format_string<Args...> format, Args &&...args) {
	intern::log_impl(format.get(), std::make_format_args(args...));
}

} // namespace tramogi::core::logging
