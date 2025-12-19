#pragma once

#include <memory>

namespace vk {
class SurfaceKHR;
namespace raii {
class SurfaceKHR;
}
} // namespace vk

namespace tramogi::graphics {

class Instance;

class Surface {
public:
	Surface();
	~Surface();
	Surface(const Surface &) = delete;
	Surface &operator=(const Surface &) = delete;
	Surface(Surface &&);
	Surface &operator=(Surface &&);

	void init(const Instance &instance, const vk::SurfaceKHR &surface);

	const vk::raii::SurfaceKHR &get_surface() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace tramogi::graphics
