#pragma once

#include "memory/checked_unique_ptr.h"

namespace tramogi::core {

template <class T> using UniquePtr = memory::CheckedUniquePtr<T>;

}
