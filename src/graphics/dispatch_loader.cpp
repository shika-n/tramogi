#include <vulkan/vulkan_raii.hpp>

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace tramogi::graphics {

void init_loader(const vk::raii::Instance &instance, const vk::raii::Device &device) {
	VULKAN_HPP_DEFAULT_DISPATCHER.init(instance, device);
}

} // namespace tramogi::graphics
