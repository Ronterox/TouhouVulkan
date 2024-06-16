#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <exception>

#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.h"

class TouhouEngine {
  public:
	const int WIDTH = 800;
	const int HEIGHT = 800;

	GLFWwindow *window;
	VkInstance instance;

	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	void initWindow() {
		LOG("Initializing window GLFW");
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		LOG("Creating window GLFW");
		window = glfwCreateWindow(WIDTH, HEIGHT, "Touhou Engine", nullptr, nullptr);
	}

	void createVkInstance() {
		LOG("Create Vulkan instance");

		VkApplicationInfo appInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pApplicationName = "Touhou Engine",
			.applicationVersion = VK_MAKE_VERSION(1, 0, 0),
			.pEngineName = "Touhou Engine",
			.engineVersion = VK_MAKE_VERSION(1, 0, 0),
			.apiVersion = VK_API_VERSION_1_0,
		};

		LOG("Obtaining required extensions for GLFW");

		uint32_t glfwExtensionCount = 0;
		const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		VkInstanceCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = 0,
			.enabledExtensionCount = glfwExtensionCount,
			.ppEnabledExtensionNames = glfwExtensions,
		};

		uint32_t vkExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, nullptr);
		list<VkExtensionProperties> extensions(vkExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, extensions.data());

		for (uint i = 0; i < glfwExtensionCount; ++i) {
			if (std::none_of(extensions.begin(), extensions.end(),
							 [&](const auto &ext) { return strcmp(ext.extensionName, glfwExtensions[i]) == 0; })) {
				ERROR("Missing required extension" + std::string(glfwExtensions[i]));
			}
		}

		LOG("Creating Vulkan instance");
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			ERROR("Failed to create Vulkan instance");
		}

		LOG("Vulkan instance created");
	}

	void initVulkan() { createVkInstance(); }

	void mainLoop() {
		LOG("Running main loop");
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		}
	}
	void cleanup() {
		LOG("Destroying Vulkan instance");
		vkDestroyInstance(instance, nullptr);

		LOG("Deleting window GLFW");
		glfwDestroyWindow(window);

		LOG("Terminating GLFW");
		glfwTerminate();
	}
};

int main() {
	TouhouEngine engine;

	try {
		engine.run();
	} catch (const std::exception &e) {
		LOGE("Exception: " << e.what());
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
