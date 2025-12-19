#include "tramogi/platform/window.h"

#include "../logging.h"
#include "tramogi/core/errors.h"
#include <cstdint>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

namespace tramogi::platform {

using core::Error;
using core::Result;

bool Window::init(uint32_t width, uint32_t height, const char *title) {
	// TODO: Check if llibdecor issue is solved. See: https://github.com/glfw/glfw/issues/2789
	glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);
	if (!glfwInit()) {
		return false;
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(width, height, title, nullptr, nullptr);

	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, resize_callback);

	return window != nullptr;
}

bool Window::should_close() {
	return glfwWindowShouldClose(window);
}

void Window::poll_events() {
	glfwPollEvents();
}

void Window::wait_events() {
	glfwWaitEvents();
}

bool Window::get_f3() {
	return glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS;
}
bool Window::get_w() {
	return glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS;
}
bool Window::get_a() {
	return glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS;
}
bool Window::get_s() {
	return glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS;
}
bool Window::get_d() {
	return glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS;
}

std::vector<const char *> Window::get_required_extensions() {
	uint32_t extension_count = 0;
	auto extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	return std::vector(extensions, extensions + extension_count);
}

Result<vk::SurfaceKHR> Window::create_surface(const vk::Instance &instance) {
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		return Error("Failed to create window surface");
	}
	return surface;
}

Dimension Window::get_size() const {
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
	DLOG("Window resized to {}x{}", width, height);
}

} // namespace tramogi::platform

