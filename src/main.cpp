#include <cstdlib>
#include <exception>
#include <print>
#include <stdexcept>

#include <GLFW/glfw3.h>
#include <vulkan/vulkan_core.h>
#include <vulkan/vulkan_raii.hpp>

#include "macros.h"

class ProjectSkyHigh {
	public:
		void run() {
			if (!glfwInit()) {
				throw std::runtime_error("Failed to initialize GLFW");
			}

			GLFWwindow *window = glfwCreateWindow(
				1280,
				720,
				"Project Sky High",
				nullptr,
				nullptr
			);

			glfwShowWindow(window);

			while (!glfwWindowShouldClose(window)) {
				glfwSwapBuffers(window);
				glfwPollEvents();
			}

			glfwDestroyWindow(window);

			glfwTerminate();

		}

	private:
		void init() {
			VkInstance instance;
			VkApplicationInfo appInfo{};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "SkyHigh Engine";

			std::println("App name: {}", appInfo.pApplicationName);
		}

		void main_loop() {

		}

		void cleanup() {

		}
};

int main() {
	DLOG("Running in DEBUG mode");

	ProjectSkyHigh skyhigh;

	try {
		skyhigh.run();
	} catch (const std::exception &e) {
		std::println(stderr, "Error: {}", e.what());
		return EXIT_FAILURE;
	}

	DLOG("Exited successfully");
	return EXIT_SUCCESS;
}
