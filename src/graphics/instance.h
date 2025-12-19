#pragma once

#include "tramogi/core/errors.h"
#include <memory>
#include <vector>

namespace vk::raii {
class Instance;
class PhysicalDevice;
} // namespace vk::raii

namespace tramogi::graphics {

class Instance {
public:
	Instance();
	~Instance();
	Instance(const Instance &) = delete;
	Instance &operator=(const Instance &) = delete;
	Instance(Instance &&);
	Instance &operator=(Instance &&);

	core::Result<> init(const std::vector<const char *> &extensions);

	std::vector<vk::raii::PhysicalDevice> get_physical_devices() const;

	const vk::raii::Instance &get_instance() const;

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace tramogi::graphics
