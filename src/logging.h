#pragma once

#ifndef NDEBUG
#include <chrono>
#include <print>

const auto log_start_time = std::chrono::high_resolution_clock::now();

#define DLOG(str, ...)                                                                             \
	std::println(                                                                                  \
		"[{:10.6f}] " str,                                                                         \
		std::chrono::duration_cast<std::chrono::duration<float>>(                                  \
			std::chrono::high_resolution_clock::now() - log_start_time                             \
		)                                                                                          \
			.count() __VA_OPT__(, ) __VA_ARGS__                                                    \
	)

inline void _keep_print_include() {
	if (false) {
		DLOG(""); // Supress unused include warning
	}
}
#else
#define DLOG(x, ...)
#endif // DEBUG
