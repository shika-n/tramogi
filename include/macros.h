#pragma once

#ifndef NDEBUG
#define DLOG(x, ...) std::println(x __VA_OPT__(, ) __VA_ARGS__)
#else
#define DLOG(x)
#endif // DEBUG
