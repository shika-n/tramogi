#pragma once

#include "tramogi/core/errors.h"
#include <cstdint>
#include <memory>

namespace vk {
enum class Format;
class PhysicalDevice;
namespace raii {
class Device;
}
} // namespace vk

namespace tramogi::graphics {

class Device;

class Texture {
public:
	Texture();
	~Texture();

	core::Result<> init(const Device &device, uint32_t width, uint32_t height, bool mipmap);

	Texture(const Texture &) = delete;
	Texture &operator=(const Texture &) = delete;
	Texture(Texture &&);
	Texture &operator=(Texture &&);

private:
	struct Impl;
	std::unique_ptr<Impl> impl;
};

} // namespace tramogi::graphics
