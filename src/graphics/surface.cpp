#include "surface.h"
#include "instance.h"
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct Surface::Impl {
	vk::raii::SurfaceKHR surface = nullptr;
};

Surface::Surface(const Instance &instance, const vk::SurfaceKHR &surface)
	: impl(std::make_unique<Impl>()) {
	impl->surface = vk::raii::SurfaceKHR(instance.get_instance(), surface);
}
Surface::~Surface() = default;
Surface::Surface(Surface &&) = default;
Surface &Surface::operator=(Surface &&) = default;

const vk::raii::SurfaceKHR &Surface::get_surface() const {
	return impl->surface;
}

} // namespace tramogi::graphics
