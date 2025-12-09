#pragma once

#ifndef NDEBUG
#include <print>
#define DLOG(x, ...) std::println(x __VA_OPT__(, ) __VA_ARGS__)

inline void _keep_print_include() {
	if (false) {
		std::println();
	}
}
#else
#define DLOG(x, ...)
#endif // DEBUG
