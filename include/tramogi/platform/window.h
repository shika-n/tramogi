#pragma once

#include <cstdint>
#include <expected>
#include <vector>

struct GLFWwindow;

typedef struct VkSurfaceKHR_T *VkSurfaceKHR;
typedef struct VkInstance_T *VkInstance;

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

	bool should_close();
	void poll_events();
	void wait_events();

	bool get_f3(); // TODO: TEMPORARY, this should be split into input class

	std::vector<const char *> get_required_extensions();
	std::expected<VkSurfaceKHR, const char *> create_surface(const VkInstance &instance);

	Dimension get_size() const;

	// TODO: Mark as deprecated
	GLFWwindow *get_glfw_window() const {
		return window;
	}

	Window &operator=(const Window &window) = delete;
	~Window();

private:
	GLFWwindow *window = nullptr;

	static void resize_callback(GLFWwindow *window, int width, int height);
};

} // namespace tramogi::platform
