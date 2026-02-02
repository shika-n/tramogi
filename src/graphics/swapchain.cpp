#include "swapchain.h"
#include "device.h"
#include "format.h"
#include "image.h"
#include "image_view.h"
#include "physical_device.h"
#include "tramogi/core/types.h"
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>
#include <vulkan/vulkan.hpp>
#include <vulkan/vulkan_raii.hpp>

namespace tramogi::graphics {

using core::Error;
using core::Result;
using core::Size;

struct Swapchain::Impl {
	vk::Extent2D extent;
	SurfaceFormat surface_format;
	vk::raii::SwapchainKHR swapchain = nullptr;
	std::vector<SwapchainImage> images;
	std::vector<ImageView> image_views;

	void create_image_views(const Device &device) {
		image_views.clear();
		image_views.reserve(images.size());
		for (const auto &image : images) {
			image_views.emplace_back(
				device,
				image,
				surface_format.format,
				vk::ImageAspectFlagBits::eColor,
				1
			);
		}
	}
};

Result<SurfaceFormat> choose_surface_format(std::span<const SurfaceFormat> available_formats) {
	constexpr std::array acceptable_formats = {
		Format::BGRA8Srgb,
		Format::RGBA8Srgb,
	};
	for (const auto acceptable_format : acceptable_formats) {
		for (const auto &format : available_formats) {
			if (format.format == acceptable_format && format.is_nonlinear) {
				return format;
			}
		}
	}
	return Error("No suitable surface format.");
}

vk::PresentModeKHR choose_present_mode(std::span<const vk::PresentModeKHR> available_present_mode) {
	for (const auto &mode : available_present_mode) {
		if (mode == vk::PresentModeKHR::eMailbox) {
			return mode;
		}
	}
	return vk::PresentModeKHR::eFifo;
}

vk::Extent2D choose_swap_extent(
	const vk::SurfaceCapabilitiesKHR &capabilities,
	const Size &dimension
) {
	if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
		return capabilities.currentExtent;
	}

	return {
		std::clamp<uint32_t>(
			dimension.x,
			capabilities.minImageExtent.width,
			capabilities.maxImageExtent.width
		),
		std::clamp<uint32_t>(
			dimension.y,
			capabilities.minImageExtent.height,
			capabilities.maxImageExtent.height
		)
	};
}

Swapchain::Swapchain(
	const PhysicalDevice &physical_device,
	const Device &device,
	const Size &window_size
)
	: impl(std::make_unique<Impl>()), physical_device(physical_device), device(device) {
	recreate(window_size);
}

Swapchain::~Swapchain() = default;
Swapchain::Swapchain(Swapchain &&) = default;

Result<uint32_t> Swapchain::get_next_image(uint32_t current_index) {
	try {
		auto [result, next_index] = impl->swapchain.acquireNextImage(
			std::numeric_limits<uint64_t>::max(),
			*device.get_present_semaphore(current_index),
			nullptr
		);

		if (result == vk::Result::eErrorOutOfDateKHR) {
			return Error("Out of date swapchain");
		}

		if (result == vk::Result::eSuccess || result == vk::Result::eSuboptimalKHR) {
			return next_index;
		}
	} catch (const vk::SystemError &e) {
		if (e.code().value() == static_cast<int>(vk::Result::eErrorOutOfDateKHR)) {
			return Error("Out of date swapchain");
		}
		throw std::runtime_error("Unexpected swapchain state");
	}

	throw std::runtime_error("Unexpected swapchain state");
}

void Swapchain::recreate(const Size &window_size) {
	impl->images.clear();
	impl->swapchain = nullptr;

	vk::SurfaceCapabilitiesKHR surface_capabilities = physical_device.get_surface_capabilities();
	std::vector<SurfaceFormat> available_formats = physical_device.get_surface_formats();
	std::vector<vk::PresentModeKHR> available_present_modes =
		physical_device.get_surface_present_modes();

	Result<SurfaceFormat> surface_format = choose_surface_format(available_formats);
	if (!surface_format) {
		throw std::runtime_error(surface_format.error());
	}
	impl->surface_format = surface_format.value();
	impl->extent = choose_swap_extent(surface_capabilities, window_size);

	auto min_image_count = std::max(3u, surface_capabilities.minImageCount);

	if (surface_capabilities.maxImageCount > 0 &&
		min_image_count > surface_capabilities.minImageCount) {
		min_image_count = surface_capabilities.maxImageCount;
	}

	uint32_t image_count = surface_capabilities.minImageCount + 1;
	if (surface_capabilities.maxImageCount > 0 &&
		image_count > surface_capabilities.maxImageCount) {
		image_count = surface_capabilities.maxImageCount;
	}

	vk::SwapchainCreateInfoKHR swapchain_create_info {
		.flags = vk::SwapchainCreateFlagsKHR(),
		.surface = physical_device.get_surface(),
		.minImageCount = min_image_count,
		.imageFormat = native(impl->surface_format.format),
		.imageColorSpace = vk::ColorSpaceKHR::eSrgbNonlinear,
		.imageExtent = impl->extent,
		.imageArrayLayers = 1,
		.imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
		.imageSharingMode = vk::SharingMode::eExclusive,
		.preTransform = surface_capabilities.currentTransform,
		.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
		.presentMode = choose_present_mode(available_present_modes),
		.clipped = vk::True,
		.oldSwapchain = nullptr,
	};

	if (physical_device.get_graphics_queue_index() != physical_device.get_present_queue_index()) {
		std::array<uint32_t, 2> indices {
			physical_device.get_graphics_queue_index(),
			physical_device.get_present_queue_index()
		};

		swapchain_create_info.imageSharingMode = vk::SharingMode::eConcurrent;
		swapchain_create_info.queueFamilyIndexCount = 2;
		swapchain_create_info.pQueueFamilyIndices = indices.data();
	} else {
		swapchain_create_info.imageSharingMode = vk::SharingMode::eExclusive;
		swapchain_create_info.queueFamilyIndexCount = 0;
		swapchain_create_info.pQueueFamilyIndices = nullptr;
	}

	impl->swapchain = vk::raii::SwapchainKHR(device.get_device(), swapchain_create_info);
	std::vector<vk::Image> images = impl->swapchain.getImages();
	impl->images.reserve(images.size());
	for (auto image : images) {
		impl->images.push_back(image);
	}

	impl->create_image_views(device);
}

const vk::raii::SwapchainKHR &Swapchain::get_swapchain() const {
	return impl->swapchain;
}

const Format &Swapchain::get_format() const {
	return impl->surface_format.format;
}

const vk::Extent2D &Swapchain::get_extent() const {
	return impl->extent;
}

const SwapchainImage &Swapchain::get_image(uint32_t index) const {
	return impl->images[index];
}

const ImageView &Swapchain::get_image_view(uint32_t index) const {
	return impl->image_views[index];
}

} // namespace tramogi::graphics
