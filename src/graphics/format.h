#pragma once

namespace vk {
enum class Format;
}

namespace tramogi::graphics {

enum class Format {
	// Types
	RG32Float,
	RGB32Float,
	RGBA16Float,
	R32UInt,

	// Colors
	RGB8Srgb,
	RGBA8Srgb,
	BGRA8Srgb,
	BGRA8Unorm,

	// Depth
	Depth32Stencil,
	Depth24Stencil,
	Depth32,

	// Aliases
	Float2 = RG32Float,
	Float3 = RGB32Float,
};

vk::Format native(Format format);

} // namespace tramogi::graphics
