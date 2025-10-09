#include <exception>
#include <print>

#include <GLFW/glfw3.h>
#include <stdexcept>

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
};

int main() {
#ifdef NDEBUG
	std::println("Running a release build");
#else
	std::println("Running a DEBUG build");
#endif

	ProjectSkyHigh skyhigh;

	try {
		skyhigh.run();
	} catch (const std::exception &e) {
		std::println(stderr, "Error: {}", e.what());
	}

	std::println("Exited successfully");
	return 0;
}
