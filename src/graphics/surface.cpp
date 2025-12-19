#include "surface.h"
#include "instance.h"
#include <memory>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

struct Surface::Impl {
	vk::raii::SurfaceKHR surface = nullptr;
};

Surface::Surface() : impl(std::make_unique<Impl>()) {}
Surface::~Surface() = default;
Surface::Surface(Surface &&) = default;
Surface &Surface::operator=(Surface &&) = default;

void Surface::init(const Instance &instance, const vk::SurfaceKHR &surface) {
	impl->surface = vk::raii::SurfaceKHR(instance.get_instance(), surface);
}

const vk::raii::SurfaceKHR &Surface::get_surface() const {
	return impl->surface;
}

} // namespace tramogi::graphics
