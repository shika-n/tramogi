#include "format.h"
#include <utility>
#include <vulkan/vulkan.hpp>

namespace tramogi::graphics {

vk::Format native(Format format) {
	switch (format) {
	case Format::RG32Float:
		return vk::Format::eR32G32Sfloat;
	case Format::RGB32Float:
		return vk::Format::eR32G32B32Sfloat;
	case Format::RGBA16Float:
		return vk::Format::eR16G16B16A16Sfloat;
	case Format::R32UInt:
		return vk::Format::eR32Sint;

	case Format::RGB8Srgb:
		return vk::Format::eR8G8B8Srgb;
	case Format::RGBA8Srgb:
		return vk::Format::eR8G8B8A8Srgb;
	case Format::BGRA8Srgb:
		return vk::Format::eB8G8R8A8Srgb;
	case Format::BGRA8Unorm:
		return vk::Format::eB8G8R8A8Unorm;

	case Format::Depth32Stencil:
		return vk::Format::eD32SfloatS8Uint;
	case Format::Depth24Stencil:
		return vk::Format::eD24UnormS8Uint;
	case Format::Depth32:
		return vk::Format::eD32Sfloat;
	}

	assert(false && "Unknown format. Did you initialize the format?");
	std::unreachable();
}

} // namespace tramogi::graphics
