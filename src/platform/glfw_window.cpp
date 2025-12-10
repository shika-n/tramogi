#include "tramogi/platform/window.h"

#include "../logging.h"
#include <cstdint>
#include <expected>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>

namespace tramogi::platform {

bool Window::init(uint32_t width, uint32_t height, const char *title) {
	if (!glfwInit()) {
		return false;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(width, height, title, nullptr, nullptr);

	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, resize_callback);

	return window != nullptr;
}

void Window::run() {
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();
	}
}

std::vector<const char *> Window::get_required_extensions() {
	uint32_t extension_count = 0;
	auto extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	return std::vector(extensions, extensions + extension_count);
}

std::expected<VkSurfaceKHR, bool> Window::create_surface(const VkInstance &instance) {
	VkSurfaceKHR surface;
	if (!glfwCreateWindowSurface(instance, window, nullptr, &surface)) {
		return std::unexpected(false);
	}
	return surface;
}

Dimension Window::get_size() {
	Dimension dimension;
	glfwGetFramebufferSize(window, &dimension.width, &dimension.height);
	return dimension;
}

Window::~Window() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Window::resize_callback(
	GLFWwindow *window,
	[[maybe_unused]] int width,
	[[maybe_unused]] int height
) {
	auto instance = reinterpret_cast<Window *>(glfwGetWindowUserPointer(window));
	instance->resized = true;
	DLOG("Resized to {}x{}", width, height);
}

} // namespace tramogi::platform

