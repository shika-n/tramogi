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
	Surface(const Instance &instance, const vk::SurfaceKHR &surface);
	~Surface();
	Surface(const Surface &) = delete;
	Surface &operator=(const Surface &) = delete;
	Surface(Surface &&);
	Surface &operator=(Surface &&);

	const vk::raii::SurfaceKHR &get_surface() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace tramogi::graphics
