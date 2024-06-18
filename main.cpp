#include <algorithm>
#include <cstring>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.h"

const int WIDTH = 800;
const int HEIGHT = 800;

const list<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
													VkDebugUtilsMessageTypeFlagsEXT messageType,
													const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
													void *pUserData) {
	if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {

		const char *severity = "";
		switch (messageSeverity) {
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
			severity = "WARNING";
			break;
		case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
			severity = "ERROR";
			break;
		default:
			severity = "INFO";
			break;
		}
		LOGE("VL_" << severity << ": " << pCallbackData->pMessage);
	}
	return VK_FALSE;
}

VkResult vkCreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
										const VkAllocationCallbacks *pAllocator,
										VkDebugUtilsMessengerEXT *pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func == nullptr) return VK_ERROR_EXTENSION_NOT_PRESENT;

	return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

void vkDestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
									 const VkAllocationCallbacks *pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func == nullptr) return;

	func(instance, debugMessenger, pAllocator);
}

class TouhouEngine {
  public:
	GLFWwindow *window;
	VkInstance instance;

	VkDebugUtilsMessengerEXT debugMessenger;

	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

		list<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char *layerName : validationLayers) {
			if (std::none_of(availableLayers.begin(), availableLayers.end(),
							 [&](const auto &layer) { return strcmp(layer.layerName, layerName) == 0; })) {
				LOGE("Missing validation layer: " << layerName);
				return false;
			}
		}
		return true;
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
		createInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
							   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
							   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = debugCallback,
		};
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;

		LOG("Setting up debug messenger");
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		LOG("Creating debug messenger");
		if (vkCreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
			ERROR("Failed to set up debug messenger");
		}
	}

	void initWindow() {
		LOG("Initializing window GLFW");
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		LOG("Creating window GLFW");
		window = glfwCreateWindow(WIDTH, HEIGHT, "Touhou Engine", nullptr, nullptr);
	}

	void verifyVkExtensions(list<const char *> glfwRequiredEXT) {
		uint32_t vkExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, nullptr);
		list<VkExtensionProperties> extensions(vkExtensionCount);
		vkEnumerateInstanceExtensionProperties(nullptr, &vkExtensionCount, extensions.data());

		for (uint i = 0; i < glfwRequiredEXT.size(); ++i) {
			if (std::none_of(extensions.begin(), extensions.end(),
							 [&](const auto &ext) { return strcmp(ext.extensionName, glfwRequiredEXT[i]) == 0; })) {
				ERROR("Missing required extension" + std::string(glfwRequiredEXT[i]));
			}
		}
	}

	void createVkInstance() {
		LOG("Create Vulkan instance");

		if (enableValidationLayers && !checkValidationLayerSupport()) {
			ERROR("Validation layers requested, but not available!");
		}

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
		const char **glfwRequiredEXT = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

		LOG("Checking for Validation Layers");
		list<const char *> glfwExtensions(glfwRequiredEXT, glfwRequiredEXT + glfwExtensionCount);
		if (enableValidationLayers) {
			glfwExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
		}

		VkInstanceCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = 0,
			.enabledExtensionCount = static_cast<uint32_t>(glfwExtensions.size()),
			.ppEnabledExtensionNames = glfwExtensions.data(),
		};

		VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();

			populateDebugMessengerCreateInfo(debugCreateInfo);
			createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
		}

		verifyVkExtensions(glfwExtensions);

		LOG("Creating Vulkan instance");
		if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
			ERROR("Failed to create Vulkan instance");
		}

		LOG("Vulkan instance created");
	}

	void initVulkan() {
		createVkInstance();
		setupDebugMessenger();
	}

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
		if (enableValidationLayers) {
			vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
		}

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
