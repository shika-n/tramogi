#include "tramogi/platform/window.h"

#include "tramogi/core/logging/logging.h"
#include "tramogi/core/types.h"
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

namespace tramogi::platform {

using core::Error;
using core::Result;
using core::Size;
using core::logging::debug_log;

Window::Window(uint32_t width, uint32_t height, const char *title) {
	// TODO: Check if llibdecor issue is solved. See: https://github.com/glfw/glfw/issues/2789
	glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);

#if defined(__linux) && !defined(NDEBUG)
	// For Renderdoc. Renderdoc has no wayland support yet
	glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#define TRAMOGI_USING_X11_DEBUG
#endif

	if (!glfwInit()) {
		throw std::runtime_error("Failed to initialize GLFW");
	}

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	window = glfwCreateWindow(width, height, title, nullptr, nullptr);

	glfwSetWindowUserPointer(window, this);
	glfwSetWindowSizeCallback(window, resize_callback);

	if (window == nullptr) {
		throw std::runtime_error("Failed to create window");
	}
}

void Window::set_key_callback(std::function<void(int, bool)> callback) {
	key_callback = callback;
	glfwSetKeyCallback(window, [](GLFWwindow *window, int, int scancode, int action, int) {
		if (action == GLFW_REPEAT) {
			return;
		}
		Window *instance = static_cast<Window *>(glfwGetWindowUserPointer(window));

#ifdef TRAMOGI_USING_X11_DEBUG
		instance->key_callback(scancode - 8, action == GLFW_PRESS);
#else
		instance->key_callback(scancode, action == GLFW_PRESS);
#endif
	});
}

void Window::set_mouse_callback(
	std::function<void(int, bool)> button_callback,
	std::function<void(double, double)> position_callback,
	std::function<void(double, double)> scroll_callback
) {
	mouse_button_callback = button_callback;
	mouse_position_callback = position_callback;
	mouse_scroll_callback = scroll_callback;

	glfwSetMouseButtonCallback(window, [](GLFWwindow *window, int button, int action, int) {
		Window *instance = static_cast<Window *>(glfwGetWindowUserPointer(window));
		instance->mouse_button_callback(button, action == GLFW_PRESS);
	});
	glfwSetCursorPosCallback(window, [](GLFWwindow *window, double x, double y) {
		Window *instance = static_cast<Window *>(glfwGetWindowUserPointer(window));
		instance->mouse_position_callback(x, y);
	});
	glfwSetScrollCallback(window, [](GLFWwindow *window, double xoffset, double yoffset) {
		Window *instance = static_cast<Window *>(glfwGetWindowUserPointer(window));
		instance->mouse_scroll_callback(xoffset, yoffset);
	});
}

void Window::request_close() {
	glfwSetWindowShouldClose(window, GLFW_TRUE);
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

std::vector<const char *> Window::get_required_extensions() {
	uint32_t extension_count = 0;
	auto extensions = glfwGetRequiredInstanceExtensions(&extension_count);
	return std::vector(extensions, extensions + extension_count);
}

vk::SurfaceKHR Window::create_surface(const vk::Instance &instance) {
	VkSurfaceKHR surface;
	if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
		throw std::runtime_error("Failed to create window surface");
	}
	return surface;
}

Size Window::get_size() const {
	Size dimension;
	glfwGetFramebufferSize(window, &dimension.x, &dimension.y);
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
	debug_log("Window resized to {}x{}", width, height);
}

} // namespace tramogi::platform

