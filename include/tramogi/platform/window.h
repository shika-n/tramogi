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
	Window() = default;
	Window(const Window &window) = delete;

	bool init(uint32_t width, uint32_t height, const char *title);

	void run();

	std::vector<const char *> get_required_extensions();

	std::expected<VkSurfaceKHR, bool> create_surface(const VkInstance &instance);

	Dimension get_size();

	Window &operator=(const Window &window) = delete;
	~Window();

private:
	GLFWwindow *window = nullptr;
	bool resized = false;

	static void resize_callback(GLFWwindow *window, int width, int height);
};

} // namespace tramogi::platform
