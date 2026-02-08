#pragma once

#include "tramogi/core/pointers.h"
#include <vector>

namespace vk::raii {
class Instance;
class PhysicalDevice;
} // namespace vk::raii

namespace tramogi::graphics {

class Instance {
public:
	Instance(const std::vector<const char *> &extensions);
	~Instance();
	Instance(const Instance &) = delete;
	Instance &operator=(const Instance &) = delete;
	Instance(Instance &&);
	Instance &operator=(Instance &&);

	std::vector<vk::raii::PhysicalDevice> get_physical_devices() const;

	const vk::raii::Instance &get_instance() const;

private:
	struct Impl;
	core::UniquePtr<Impl> impl;
};

} // namespace tramogi::graphics
