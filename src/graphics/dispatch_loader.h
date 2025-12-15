#pragma once

namespace vk::raii {
class Instance;
class Device;
} // namespace vk::raii

namespace tramogi::graphics {

void init_loader(const vk::raii::Instance &instance, const vk::raii::Device &device);

}
