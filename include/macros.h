#pragma once

#ifndef NDEBUG
#define DLOG(x) (std::println(x))
#else
#define DLOG(x)
#endif // DEBUG

