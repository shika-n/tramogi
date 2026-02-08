#pragma once

#include <cassert>
#include <memory>

namespace tramogi::core::memory {

template <class T> class CheckedUniquePtr {
public:
	CheckedUniquePtr() = default;
	CheckedUniquePtr(std::unique_ptr<T> &&ptr) : ptr(std::move(ptr)) {}
	~CheckedUniquePtr() = default;
	CheckedUniquePtr(const CheckedUniquePtr &) = delete;
	CheckedUniquePtr &operator=(const CheckedUniquePtr &) = delete;
	CheckedUniquePtr(CheckedUniquePtr &&) = default;
	CheckedUniquePtr &operator=(CheckedUniquePtr &&) = default;

	T *get() const noexcept {
		return ptr.get();
	}

	T *operator->() const {
		assert(ptr && "You tried to access a null UniquePtr");
		return ptr.get();
	}

	T &operator*() const {
		assert(ptr && "You tried to dereference a null UniquePtr");
		return *ptr;
	}

	explicit operator bool() const noexcept {
		return static_cast<bool>(ptr);
	}

private:
	std::unique_ptr<T> ptr = nullptr;
};

} // namespace tramogi::core::memory

