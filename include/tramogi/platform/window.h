#pragma once

#include "tramogi/core/errors.h"
#include <cstdint>
#include <functional>
#include <vector>

struct GLFWwindow;

namespace vk {
class SurfaceKHR;
class Instance;
} // namespace vk

namespace tramogi::platform {

struct Dimension {
	int width;
	int height;
};

// TODO: rule 1-3-5
class Window {
public:
	bool resized = false;

	Window() = default;
	Window(const Window &window) = delete;

	bool init(uint32_t width, uint32_t height, const char *title);

	void set_key_callback(std::function<void(int, bool)> callback);

	void request_close();
	bool should_close();
	void poll_events();
	void wait_events();

	std::vector<const char *> get_required_extensions();
	core::Result<vk::SurfaceKHR> create_surface(const vk::Instance &instance);

	Dimension get_size() const;

	// TODO: Mark as deprecated
	GLFWwindow *get_glfw_window() const {
		return window;
	}

	Window &operator=(const Window &window) = delete;
	~Window();

private:
	GLFWwindow *window = nullptr;

	std::function<void(int, bool)> key_callback;

	static void resize_callback(GLFWwindow *window, int width, int height);
};

} // namespace tramogi::platform
