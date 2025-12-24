#include "tramogi/core/logging/logging.h"
#include <chrono>
#include <format>
#include <print>

namespace tramogi::core::logging {

const auto log_start_time = std::chrono::high_resolution_clock::now();

namespace intern {

void log_impl(std::string_view format, std::format_args args) {
	float time = std::chrono::duration_cast<std::chrono::duration<float>>(
					 std::chrono::high_resolution_clock::now() - log_start_time
	)
					 .count();
	std::string formatted = std::vformat(format, args);

	std::println("[{:10.6f}] {}", time, formatted);
}

} // namespace intern

} // namespace tramogi::core::logging

