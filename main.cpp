#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <map>
#include <optional>
#include <set>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.h"

constexpr int WIDTH = 800;
constexpr int HEIGHT = 600;

constexpr int MAX_FRAMES_IN_FLIGHT = 2;

const list<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
#ifdef NDEBUG
constexpr bool enableValidationLayers = false;
#else
constexpr bool enableValidationLayers = true;
#endif

const list<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct UniformBufferObject {
	alignas(16) glm::mat4 model;
	alignas(16) glm::mat4 view;
	alignas(16) glm::mat4 proj;
};

struct Vertex {
	glm::vec2 pos;
	glm::vec3 color;

	static VkVertexInputBindingDescription getBindingDescription() {
		VkVertexInputBindingDescription bindingDescription{
			.binding = 0,
			.stride = sizeof(Vertex),
			.inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
		};
		return bindingDescription;
	}

	static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
		std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
		attributeDescriptions[0].binding = 0;
		attributeDescriptions[0].location = 0;
		attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
		attributeDescriptions[0].offset = offsetof(Vertex, pos);

		attributeDescriptions[1].binding = 0;
		attributeDescriptions[1].location = 1;
		attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
		attributeDescriptions[1].offset = offsetof(Vertex, color);
		return attributeDescriptions;
	}
};

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily, presentFamily;

	bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	list<VkSurfaceFormatKHR> formats;
	list<VkPresentModeKHR> presentModes;
};

static list<char> readFile(const std::string &filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);
	VALIDATE(file.is_open(), "Failed to open file: " + filename);

	size_t fileSize = static_cast<size_t>(file.tellg());
	list<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);
	file.close();

	return buffer;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(const VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
													const VkDebugUtilsMessageTypeFlagsEXT messageType,
													const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
													[[gnu::unused]] void *pUserData) {
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
		LOGE("VK_" << messageType << "_EXT: " << severity << ": " << pCallbackData->pMessage);
	}
	return VK_FALSE;
}

VkResult vkCreateDebugUtilsMessengerEXT(const VkInstance instance,
										const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
										const VkAllocationCallbacks *pAllocator,
										VkDebugUtilsMessengerEXT *pDebugMessenger) {
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func == VK_NULL_HANDLE) return VK_ERROR_EXTENSION_NOT_PRESENT;

	return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
}

void vkDestroyDebugUtilsMessengerEXT(const VkInstance instance, const VkDebugUtilsMessengerEXT debugMessenger,
									 const VkAllocationCallbacks *pAllocator) {
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func == VK_NULL_HANDLE) return;

	func(instance, debugMessenger, pAllocator);
}

class TouhouEngine {
  public:
	GLFWwindow *window;
	VkInstance instance;
	VkPipeline graphicsPipeline;

	VkRenderPass renderPass;
	VkPipelineLayout pipelineLayout;

	VkCommandPool commandPool;
	list<VkCommandBuffer> commandBuffers;

	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;

	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	list<VkImage> swapChainImages;
	list<VkImageView> swapChainImageViews;
	list<VkFramebuffer> swapChainFramebuffer;

	VkQueue graphicsQueue, presentQueue;
	VkDevice device;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
	VkDebugUtilsMessengerEXT debugMessenger;

	list<VkSemaphore> imageAvailableSemaphores;
	list<VkSemaphore> renderFinishedSemaphores;
	list<VkFence> waitFrameFences;

	uint32_t currentFrame = 0;
	bool framebufferResized = false;

	VkBuffer vertexBuffer, indexBuffer;
	VkDeviceMemory vertexBufferMemory, indexBufferMemory;

	list<VkBuffer> uniformBuffers;
	list<VkDeviceMemory> uniformBuffersMemory;
	list<void *> uniformBuffersMapped;

	VkDescriptorPool descriptorPool;
	VkDescriptorSetLayout descriptorSetLayout;
	list<VkDescriptorSet> descriptorSets;

	void run() {
		initWindow();
		initVulkan();
		mainLoop();
		cleanup();
	}

  private:
	const list<Vertex> vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
								   {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
								   {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
								   {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
	const list<uint16_t> indices = {0, 1, 2, 2, 3, 0};

	bool checkValidationLayerSupport() {
		uint32_t layerCount;
		vkEnumerateInstanceLayerProperties(&layerCount, VK_NULL_HANDLE);

		list<VkLayerProperties> availableLayers(layerCount);
		vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

		for (const char *layerName : validationLayers) {
			if (std::none_of(availableLayers.begin(), availableLayers.end(),
							 [&](const auto &layer) { return IS_STR_EQUAL(layer.layerName, layerName); })) {
				LOGE("Missing validation layer: " << layerName);
				return false;
			}
		}
		return true;
	}

	void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
		createInfo = {
			.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
							   VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
							   VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
			.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
						   VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
			.pfnUserCallback = debugCallback,
			.pUserData = VK_NULL_HANDLE,
		};
	}

	void setupDebugMessenger() {
		if (!enableValidationLayers) return;

		LOG("Setting up debug messenger");
		VkDebugUtilsMessengerCreateInfoEXT createInfo;
		populateDebugMessengerCreateInfo(createInfo);

		LOG("Creating debug messenger");
		VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &createInfo, VK_NULL_HANDLE, &debugMessenger),
				 "Failed to set up debug messenger");
		LOG("Debug messenger created");
	}

	void initWindow() {
		LOG("Initializing window GLFW");
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		LOG("Creating window GLFW");
		glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
		window = glfwCreateWindow(WIDTH, HEIGHT, "Touhou Engine", VK_NULL_HANDLE, VK_NULL_HANDLE);
		glfwSetWindowUserPointer(window, this);
		glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
	}

	static void framebufferResizeCallback(GLFWwindow *window, [[gnu::unused]] int width, [[gnu::unused]] int height) {
		const auto app = reinterpret_cast<TouhouEngine *>(glfwGetWindowUserPointer(window));
		app->framebufferResized = true;
	}

	void verifyVkExtensions(list<const char *> glfwRequiredEXT) {
		uint32_t vkExtensionCount = 0;
		vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &vkExtensionCount, VK_NULL_HANDLE);
		list<VkExtensionProperties> extensions(vkExtensionCount);
		vkEnumerateInstanceExtensionProperties(VK_NULL_HANDLE, &vkExtensionCount, extensions.data());

		const auto CONTAINS = [&extensions](const char *ext) {
			return std::any_of(extensions.begin(), extensions.end(),
							   [&](const auto &extension) { return IS_STR_EQUAL(extension.extensionName, ext); });
		};

		for (uint i = 0; i < glfwRequiredEXT.size(); ++i) {
			VALIDATE(CONTAINS(glfwRequiredEXT[i]), "Missing required extension" + std::string(glfwRequiredEXT[i]));
		}
	}

	void createVkInstance() {
		LOG("Create Vulkan instance");

		VALIDATE(!enableValidationLayers || checkValidationLayerSupport(),
				 "Validation layers requested, but not available!");

		const VkApplicationInfo appInfo{
			.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
			.pNext = VK_NULL_HANDLE,
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
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.pApplicationInfo = &appInfo,
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = VK_NULL_HANDLE,
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
		VK_CHECK(vkCreateInstance(&createInfo, VK_NULL_HANDLE, &instance), "Failed to create Vulkan instance");

		LOG("Vulkan instance created");
	}

	void pickPhysicalDevice() {
		uint32_t physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, VK_NULL_HANDLE);

		list<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

		VALIDATE(physicalDeviceCount > 0, "No suitable GPU found!");

		std::multimap<int, VkPhysicalDevice> candidates;
		for (const auto &device : physicalDevices) {
			int score = getDeviceScore(device);
			candidates.insert(std::make_pair(score, device));
		}

		LOG(candidates.size() << " GPUs found");
		VALIDATE(candidates.rbegin()->first > 0, "Failed to find a suitable GPU score!");
		physicalDevice = candidates.rbegin()->second;

		VALIDATE(isDeviceSuitable(physicalDevice), "GPU is not suitable");
		LOG("GPU successfully selected with a score of " << candidates.rbegin()->first);
	}

	QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice device) {
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, VK_NULL_HANDLE);

		list<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

		QueueFamilyIndices indices;
		for (uint32_t i = 0; i < queueFamilyCount && !indices.isComplete(); ++i) {
			VkBool32 presentSupport = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

			if (presentSupport) {
				indices.presentFamily = i;
			}

			if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
				indices.graphicsFamily = i;
			}
		}
		return indices;
	}

	bool isDeviceSuitable(const VkPhysicalDevice device) {
		QueueFamilyIndices indices = findQueueFamilies(device);
		const bool extensionsSupported = checkDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}
		return swapChainAdequate && indices.isComplete();
	}

	bool checkDeviceExtensionSupport(const VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, VK_NULL_HANDLE, &extensionCount, VK_NULL_HANDLE);

		list<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, VK_NULL_HANDLE, &extensionCount, availableExtensions.data());

		for (const auto &ext : deviceExtensions) {
			if (std::none_of(availableExtensions.begin(), availableExtensions.end(),
							 [&](const auto &extension) { return IS_STR_EQUAL(extension.extensionName, ext); })) {
				return false;
			}
		}
		return true;
	}

	int getDeviceScore(const VkPhysicalDevice device) {
		LOG("Obtaining device properties");

		VkPhysicalDeviceProperties deviceProperties;
		vkGetPhysicalDeviceProperties(device, &deviceProperties);

		VkPhysicalDeviceFeatures deviceFeatures;
		vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

		const char *deviceName = deviceProperties.deviceName;
		LOG("Checking for geometry shader support for " << deviceName);

		if (!deviceFeatures.geometryShader) return 0;

		int score = 0;
		LOG("Checking for GPU type for " << deviceName);
		if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
			score += 1000;
		}

		score += deviceProperties.limits.maxImageDimension2D;

		LOG("Device " << deviceName << " has a score of " << score);

		return score;
	}

	void createLogicalDevice() {
		LOG("Creating logical device");
		const QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		list<VkDeviceQueueCreateInfo> queueCreateInfos;
		const std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		const float queuePriority = 1.0f;
		for (const uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.pNext = VK_NULL_HANDLE,
				.flags = 0,
				.queueFamilyIndex = queueFamily,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority,
			};
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkDeviceCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledLayerCount = 0,
			.ppEnabledLayerNames = VK_NULL_HANDLE,
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
			.pEnabledFeatures = VK_NULL_HANDLE,
		};

		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}

		VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, VK_NULL_HANDLE, &device),
				 "Failed to create logical device!");

		LOG("Obtaining graphicsFamily queue");
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);

		LOG("Obtaining presentFamily queue");
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

		LOG("Logical device created");
	}

	void createWindowSurface() {
		LOG("Creating window surface");
		VK_CHECK(glfwCreateWindowSurface(instance, window, VK_NULL_HANDLE, &surface),
				 "Failed to create window surface!");
		LOG("Window surface created");
	}

	SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice device) {
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, VK_NULL_HANDLE);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, VK_NULL_HANDLE);

		if (presentModeCount != 0) {
			details.presentModes.resize(presentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
		}

		return details;
	}

	VkSurfaceFormatKHR chooseSwapSurfaceFormat(const list<VkSurfaceFormatKHR> &availableFormats) {
		for (const auto &availableFormat : availableFormats) {
			if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
				availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
				return availableFormat;
			}
		}
		return availableFormats[0];
	}

	VkPresentModeKHR chooseSwapPresentMode(const list<VkPresentModeKHR> &availablePresentModes) {
		for (const auto &availablePresentMode : availablePresentModes) {
			if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
				return availablePresentMode;
			}
		}
		return VK_PRESENT_MODE_FIFO_KHR;
	}

	VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
		if (capabilities.currentExtent.width != UINT32_MAX) {
			return capabilities.currentExtent;
		}

		int width, height;
		glfwGetFramebufferSize(window, &width, &height);

		VkExtent2D actualExtent = {
			static_cast<uint32_t>(width),
			static_cast<uint32_t>(height),
		};

		actualExtent.width =
			std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
		actualExtent.height =
			std::clamp(actualExtent.height, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);

		return actualExtent;
	}

	void createSwapChain() {
		LOG("Querying swap chain support details for creation");
		const SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		LOG("Choosing swap chain details");
		const VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		const VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		const VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		VkSwapchainCreateInfoKHR createInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,

			.surface = surface,
			.minImageCount = swapChainSupport.capabilities.minImageCount + 1,
			.imageFormat = surfaceFormat.format,
			.imageColorSpace = surfaceFormat.colorSpace,
			.imageExtent = extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

			.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,

			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = VK_NULL_HANDLE,

			.preTransform = swapChainSupport.capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

			.presentMode = presentMode,
			.clipped = VK_TRUE,

			.oldSwapchain = VK_NULL_HANDLE,
		};

		const QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		if (indices.graphicsFamily != indices.presentFamily) {
			const uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		LOG("Creating swap chain");
		VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, VK_NULL_HANDLE, &swapChain), "Failed to create swap chain!");

		LOG("Swap chain created");
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		LOG("Obtaining swap chain images");
		uint32_t imageCount;
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, VK_NULL_HANDLE);

		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		LOG("Swap chain images obtained");
	}

	void createImageViews() {
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); ++i) {
			const VkImageViewCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
				.pNext = VK_NULL_HANDLE,
				.flags = 0,

				.image = swapChainImages[i],
				.viewType = VK_IMAGE_VIEW_TYPE_2D,
				.format = swapChainImageFormat,

				.components =
					{
						.r = VK_COMPONENT_SWIZZLE_IDENTITY,
						.g = VK_COMPONENT_SWIZZLE_IDENTITY,
						.b = VK_COMPONENT_SWIZZLE_IDENTITY,
						.a = VK_COMPONENT_SWIZZLE_IDENTITY,
					},
				.subresourceRange =
					{
						.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
						.baseMipLevel = 0,
						.levelCount = 1,
						.baseArrayLayer = 0,
						.layerCount = 1,
					},
			};

			VK_CHECK(vkCreateImageView(device, &createInfo, VK_NULL_HANDLE, &swapChainImageViews[i]),
					 "Failed to create image views!");
			LOG("Image view created");
		}
	}

	VkShaderModule createShaderModule(const list<char> &code) {
		LOG("Creating shader module");

		const VkShaderModuleCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.codeSize = code.size(),
			.pCode = reinterpret_cast<const uint32_t *>(code.data()),
		};

		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device, &createInfo, VK_NULL_HANDLE, &shaderModule),
				 "Failed to create shader module!");

		return shaderModule;
	}

	void createGraphicsPipeline() {
		LOG("Initializing graphics pipeline creation");

		const auto vertShaderCode = readFile("shaders/shader_vert.spv");
		const auto fragShaderCode = readFile("shaders/shader_frag.spv");

		const VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
		const VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

		const VkPipelineShaderStageCreateInfo vertShaderStageInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.stage = VK_SHADER_STAGE_VERTEX_BIT,
			.module = vertShaderModule,
			.pName = "main",
			.pSpecializationInfo = VK_NULL_HANDLE,
		};

		const VkPipelineShaderStageCreateInfo fragShaderStageInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.stage = VK_SHADER_STAGE_FRAGMENT_BIT,
			.module = fragShaderModule,
			.pName = "main",
			.pSpecializationInfo = VK_NULL_HANDLE,
		};

		const VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
		const list<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

		const VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
			.pDynamicStates = dynamicStates.data(),
		};

		const auto bindingDescriptions = Vertex::getBindingDescription();
		const auto attributeDescriptions = Vertex::getAttributeDescriptions();

		const VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.vertexBindingDescriptionCount = 1,
			.pVertexBindingDescriptions = &bindingDescriptions,
			.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size()),
			.pVertexAttributeDescriptions = attributeDescriptions.data(),
		};

		const VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};

		const VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.viewportCount = 1,
			.pViewports = VK_NULL_HANDLE,
			.scissorCount = 1,
			.pScissors = VK_NULL_HANDLE,
		};

		const VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,

			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,

			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,

			.depthBiasEnable = VK_FALSE,
			.depthBiasConstantFactor = 0.0f,
			.depthBiasClamp = 0.0f,
			.depthBiasSlopeFactor = 0.0f,

			.lineWidth = 1.0f,
		};

		const VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,

			.sampleShadingEnable = VK_FALSE,
			.minSampleShading = 1.0f,
			.pSampleMask = VK_NULL_HANDLE,

			.alphaToCoverageEnable = VK_FALSE,
			.alphaToOneEnable = VK_FALSE,
		};

		// finalColor.rgb = newColor.rgb * newColor.a + oldColor.rgb * (1 - newColor.a)
		// finalColor.a = newColor.a * 1 + oldColor.a * 0
		const VkPipelineColorBlendAttachmentState colorBlendAttachment{
			.blendEnable = VK_TRUE,

			.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
			.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
			.colorBlendOp = VK_BLEND_OP_ADD,

			.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
			.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
			.alphaBlendOp = VK_BLEND_OP_ADD,

			.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
							  VK_COLOR_COMPONENT_A_BIT,
		};

		const VkPipelineColorBlendStateCreateInfo colorBlending{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.logicOpEnable = VK_FALSE,
			.logicOp = VK_LOGIC_OP_CLEAR,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachment,
			.blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},
		};

		const VkPipelineLayoutCreateInfo pipelineLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.setLayoutCount = 1,
			.pSetLayouts = &descriptorSetLayout,
			.pushConstantRangeCount = 0,
			.pPushConstantRanges = VK_NULL_HANDLE,
		};

		LOG("Creating graphics pipeline layout");
		VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, VK_NULL_HANDLE, &pipelineLayout),
				 "Failed to create pipeline layout!");

		const VkGraphicsPipelineCreateInfo pipelineInfo{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,

			.stageCount = 2,
			.pStages = shaderStages,

			.pVertexInputState = &vertexInputInfo,
			.pInputAssemblyState = &inputAssembly,
			.pTessellationState = VK_NULL_HANDLE,
			.pViewportState = &viewportState,
			.pRasterizationState = &rasterizer,
			.pMultisampleState = &multisampling,
			.pDepthStencilState = VK_NULL_HANDLE,
			.pColorBlendState = &colorBlending,
			.pDynamicState = &dynamicStateCreateInfo,

			.layout = pipelineLayout,
			.renderPass = renderPass,
			.subpass = 0,

			.basePipelineHandle = VK_NULL_HANDLE,
			.basePipelineIndex = -1,
		};

		LOG("Creating graphics pipeline");
		VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, VK_NULL_HANDLE, &graphicsPipeline),
				 "Failed to create graphics pipeline!");

		LOG("Graphics pipeline created, now liberating resources");
		vkDestroyShaderModule(device, fragShaderModule, VK_NULL_HANDLE);
		vkDestroyShaderModule(device, vertShaderModule, VK_NULL_HANDLE);

		LOG("Shader modules destroyed");
	}

	void createRenderPass() {
		LOG("Initializing render pass creation");

		const VkAttachmentDescription colorAttachment{
			.flags = 0,

			.format = swapChainImageFormat,
			.samples = VK_SAMPLE_COUNT_1_BIT,

			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,

			.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
			.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,

			.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
			.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
		};

		const VkAttachmentReference colorAttachmentRef{
			.attachment = 0,
			.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		};

		const VkSubpassDescription subpass{
			.flags = 0,
			.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
			.inputAttachmentCount = 0,
			.pInputAttachments = VK_NULL_HANDLE,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachmentRef,
			.pResolveAttachments = VK_NULL_HANDLE,
			.pDepthStencilAttachment = VK_NULL_HANDLE,
			.preserveAttachmentCount = 0,
			.pPreserveAttachments = VK_NULL_HANDLE,
		};

		const VkRenderPassCreateInfo renderPassInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.attachmentCount = 1,
			.pAttachments = &colorAttachment,
			.subpassCount = 1,
			.pSubpasses = &subpass,
			.dependencyCount = 0,
			.pDependencies = VK_NULL_HANDLE,
		};

		LOG("Creating render pass");
		VK_CHECK(vkCreateRenderPass(device, &renderPassInfo, VK_NULL_HANDLE, &renderPass),
				 "Failed to create render pass!");

		LOG("Render pass created");
	}

	void createFramebuffers() {
		LOG("Creating framebuffers");
		swapChainFramebuffer.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); ++i) {
			const VkImageView attachments[] = {swapChainImageViews[i]};

			const VkFramebufferCreateInfo framebufferCreateInfo{
				.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
				.pNext = VK_NULL_HANDLE,
				.flags = 0,
				.renderPass = renderPass,
				.attachmentCount = 1,
				.pAttachments = attachments,
				.width = swapChainExtent.width,
				.height = swapChainExtent.height,
				.layers = 1,
			};

			VK_CHECK(vkCreateFramebuffer(device, &framebufferCreateInfo, VK_NULL_HANDLE, &swapChainFramebuffer[i]),
					 "Failed to create framebuffer!");
		}
		LOG("Framebuffers created");
	}

	void createCommandPool() {
		LOG("Creating command pool");

		const QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

		const VkCommandPoolCreateInfo poolInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
			.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value(),
		};

		VK_CHECK(vkCreateCommandPool(device, &poolInfo, VK_NULL_HANDLE, &commandPool),
				 "Failed to create command pool!");

		LOG("Command pool created");
	}

	void createCommandBuffers() {
		LOG("Creating command buffer");
		commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);

		const VkCommandBufferAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.commandPool = commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = MAX_FRAMES_IN_FLIGHT,
		};

		VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data()),
				 "Failed to allocate command buffers!");

		LOG("Command buffer created");
	}

	void recordCommandBuffer(const VkCommandBuffer commandBuffer, const uint32_t imageIndex) {
		const VkCommandBufferBeginInfo beginInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.pInheritanceInfo = VK_NULL_HANDLE,
		};

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo), "Failed to begin recording command buffer!");

		const VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};

		const VkRenderPassBeginInfo renderPassInfo{
			.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			.pNext = VK_NULL_HANDLE,
			.renderPass = renderPass,
			.framebuffer = swapChainFramebuffer[imageIndex],
			.renderArea =
				{
					.offset = {0, 0},
					.extent = swapChainExtent,
				},
			.clearValueCount = 1,
			.pClearValues = &clearColor,
		};

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

		const VkDeviceSize offsets[1] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, &vertexBuffer, offsets);
		vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT16);

		const VkViewport viewport{
			.x = 0.0f,
			.y = 0.0f,
			.width = static_cast<float>(swapChainExtent.width),
			.height = static_cast<float>(swapChainExtent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};

		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		const VkRect2D scissor{
			.offset = {0, 0},
			.extent = swapChainExtent,
		};

		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1,
								&descriptorSets[currentFrame], 0, VK_NULL_HANDLE);

		vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

		vkCmdEndRenderPass(commandBuffer);

		VK_CHECK(vkEndCommandBuffer(commandBuffer), "Failed to record command buffer!");
	}

	void createSyncObjects() {
		LOG("Creating synchronization objects");
		imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
		waitFrameFences.resize(MAX_FRAMES_IN_FLIGHT);

		VkSemaphoreCreateInfo semaphoreInfo{
			.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
		};
		VkFenceCreateInfo fenceInfo{
			.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = VK_FENCE_CREATE_SIGNALED_BIT,
		};

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			if (vkCreateSemaphore(device, &semaphoreInfo, VK_NULL_HANDLE, &imageAvailableSemaphores[i]) != VK_SUCCESS ||
				vkCreateSemaphore(device, &semaphoreInfo, VK_NULL_HANDLE, &renderFinishedSemaphores[i]) != VK_SUCCESS ||
				vkCreateFence(device, &fenceInfo, VK_NULL_HANDLE, &waitFrameFences[i]) != VK_SUCCESS) {
				ERROR("Failed to create synchronization objects for a frame!");
			}
		}

		LOG("Synchronization objects created");
	}

	uint32_t findMemoryType(const uint32_t typeFilter, const VkMemoryPropertyFlags properties) {
		VkPhysicalDeviceMemoryProperties memoryProperties;
		vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);

		for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
			if (typeFilter & (1 << i) && (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		ERROR("Failed to find suitable memory type!");
	}

	void copyBuffer(const VkBuffer srcBuffer, const VkBuffer dstBuffer, const VkDeviceSize size) {
		LOG("Allocating command buffer");
		const VkCommandBufferAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.commandPool = commandPool,
			.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
			.commandBufferCount = 1,
		};

		VkCommandBuffer commandBuffer;
		vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

		const VkCommandBufferBeginInfo beginInfo{.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
												 .pNext = VK_NULL_HANDLE,
												 .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
												 .pInheritanceInfo = VK_NULL_HANDLE};

		vkBeginCommandBuffer(commandBuffer, &beginInfo);

		const VkBufferCopy copyRegion{
			.srcOffset = 0,
			.dstOffset = 0,
			.size = size,
		};
		vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

		vkEndCommandBuffer(commandBuffer);

		LOG("Submitting command buffer");

		const VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = VK_NULL_HANDLE,
			.waitSemaphoreCount = 0,
			.pWaitSemaphores = VK_NULL_HANDLE,
			.pWaitDstStageMask = VK_NULL_HANDLE,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			.signalSemaphoreCount = 0,
			.pSignalSemaphores = VK_NULL_HANDLE,
		};

		vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
		vkQueueWaitIdle(graphicsQueue);

		vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
	}

	void createBuffer(const VkDeviceSize size, const VkBufferUsageFlags usage, const VkMemoryPropertyFlags properties,
					  VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
		LOG("Creating single vertex buffer");
		const VkBufferCreateInfo bufferInfo{
			.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.size = size,
			.usage = usage,
			.sharingMode = VK_SHARING_MODE_EXCLUSIVE,
			.queueFamilyIndexCount = 0,
			.pQueueFamilyIndices = VK_NULL_HANDLE,
		};

		VK_CHECK(vkCreateBuffer(device, &bufferInfo, VK_NULL_HANDLE, &buffer), "Failed to create vertex buffer!");
		LOG("Vertex buffer created");

		VkMemoryRequirements memRequirements;
		vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

		const VkMemoryAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.allocationSize = memRequirements.size,
			.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties),
		};

		LOG("Allocating vertex buffer memory");
		VK_CHECK(vkAllocateMemory(device, &allocInfo, VK_NULL_HANDLE, &bufferMemory),
				 "Failed to allocate vertex buffer memory!");

		LOG("Binding vertex buffer memory");
		vkBindBufferMemory(device, buffer, bufferMemory, 0);
	}

	void createAndAllocBuffer(const VkDeviceSize bufferSize, const VkBufferUsageFlags usage, const void *bufferData,
							  VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
		LOG("Allocating and staging buffer");

		VkBuffer stagingBuffer;
		VkDeviceMemory stagingBufferMemory;
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stagingBuffer,
					 stagingBufferMemory);

		LOG("Mapping vertex buffer");
		void *data;
		vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &data);
		memcpy(data, bufferData, (size_t)bufferSize);
		vkUnmapMemory(device, stagingBufferMemory);

		LOG("Creating main vertex buffer");
		createBuffer(bufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buffer,
					 bufferMemory);

		LOG("Copying buffer");
		copyBuffer(stagingBuffer, buffer, bufferSize);

		LOG("Destroying staging buffer");
		vkDestroyBuffer(device, stagingBuffer, VK_NULL_HANDLE);
		vkFreeMemory(device, stagingBufferMemory, VK_NULL_HANDLE);
	}

	void createVertexBuffer() {
		LOG("Creating all vertex buffer");
		const VkDeviceSize bufferSize = sizeof(vertices[0]) * vertices.size();
		createAndAllocBuffer(bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertices.data(), vertexBuffer,
							 vertexBufferMemory);
	}

	void createIndexBuffer() {
		LOG("Creating all vertex buffer");
		const VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();
		createAndAllocBuffer(bufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indices.data(), indexBuffer,
							 indexBufferMemory);
	}

	void createDescriptorSetLayout() {
		LOG("Creating descriptor set layout");

		const VkDescriptorSetLayoutBinding uboLayoutBinding{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
			.pImmutableSamplers = VK_NULL_HANDLE,
		};

		const VkDescriptorSetLayoutCreateInfo layoutInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.bindingCount = 1,
			.pBindings = &uboLayoutBinding,
		};

		VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, VK_NULL_HANDLE, &descriptorSetLayout),
				 "Failed to create descriptor set layout!");

		LOG("Descriptor set layout created");
	}

	void createUniformBuffers() {
		LOG("Creating uniform buffers");
		const VkDeviceSize bufferSize = sizeof(UniformBufferObject);

		uniformBuffers.resize(MAX_FRAMES_IN_FLIGHT);
		uniformBuffersMemory.resize(MAX_FRAMES_IN_FLIGHT);
		uniformBuffersMapped.resize(MAX_FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			createBuffer(bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
						 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, uniformBuffers[i],
						 uniformBuffersMemory[i]);

			vkMapMemory(device, uniformBuffersMemory[i], 0, bufferSize, 0, &uniformBuffersMapped[i]);
		}

		LOG("Uniform buffers created");
	}

	void createDescriptorPool() {
		LOG("Creating descriptor pool");

		VkDescriptorPoolSize poolSize{
			.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			.descriptorCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
		};

		VkDescriptorPoolCreateInfo poolInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.flags = 0,
			.maxSets = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
			.poolSizeCount = 1,
			.pPoolSizes = &poolSize,
		};

		VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, VK_NULL_HANDLE, &descriptorPool),
				 "Failed to create descriptor pool!");
	}

	void createDescriptorSets() {
		LOG("Creating descriptor sets");

		list<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, descriptorSetLayout);
		VkDescriptorSetAllocateInfo allocInfo{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.pNext = VK_NULL_HANDLE,
			.descriptorPool = descriptorPool,
			.descriptorSetCount = static_cast<uint32_t>(MAX_FRAMES_IN_FLIGHT),
			.pSetLayouts = layouts.data(),
		};
		descriptorSets.resize(MAX_FRAMES_IN_FLIGHT);

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, descriptorSets.data()),
				 "Failed to allocate descriptor sets!");

		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			VkDescriptorBufferInfo bufferInfo{
				.buffer = uniformBuffers[i],
				.offset = 0,
				.range = sizeof(UniformBufferObject),
			};

			VkWriteDescriptorSet writeDescriptorSet{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.pNext = VK_NULL_HANDLE,
				.dstSet = descriptorSets[i],
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
				.pImageInfo = VK_NULL_HANDLE,
				.pBufferInfo = &bufferInfo,
				.pTexelBufferView = VK_NULL_HANDLE,
			};

			vkUpdateDescriptorSets(device, 1, &writeDescriptorSet, 0, VK_NULL_HANDLE);
		}
	}

	void initVulkan() {
		createVkInstance();
		setupDebugMessenger();

		createWindowSurface();
		pickPhysicalDevice();
		createLogicalDevice();

		createSwapChain();
		createImageViews();

		createRenderPass();
		createDescriptorSetLayout();

		createGraphicsPipeline();
		createFramebuffers();

		createCommandPool();
		createCommandBuffers();

		createVertexBuffer();
		createIndexBuffer();

		createUniformBuffers();
		createDescriptorPool();
		createDescriptorSets();

		createSyncObjects();
	}

	void updateUniformBuffer(const uint32_t currentFrame) {
		static auto startTime = std::chrono::high_resolution_clock::now();

		const auto currentTime = std::chrono::high_resolution_clock::now();
		const float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

		UniformBufferObject ubo{
			.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
			.proj = glm::perspective(glm::radians(45.0f), swapChainExtent.width / (float)swapChainExtent.height, 0.1f,
									 10.0f),
		};
		ubo.proj[1][1] *= -1;

		memcpy(uniformBuffersMapped[currentFrame], &ubo, sizeof(ubo));
	}

	void drawNextFrame() {
		vkWaitForFences(device, 1, &waitFrameFences[currentFrame], VK_TRUE, UINT64_MAX);

		updateUniformBuffer(currentFrame);

		uint32_t imageIndex;
		VkResult result = vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphores[currentFrame],
												VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return;
		} else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			ERROR("Failed to acquire next image for frame!");
		}

		vkResetFences(device, 1, &waitFrameFences[currentFrame]);

		vkResetCommandBuffer(commandBuffers[currentFrame], 0);
		recordCommandBuffer(commandBuffers[currentFrame], imageIndex);

		VkPipelineStageFlags waitStagesMask[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

		VkSubmitInfo submitInfo{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			.pNext = VK_NULL_HANDLE,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &imageAvailableSemaphores[currentFrame],
			.pWaitDstStageMask = waitStagesMask,
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffers[currentFrame],
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = &renderFinishedSemaphores[currentFrame],
		};

		VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, waitFrameFences[currentFrame]),
				 "Failed to submit draw command buffer!");

		VkPresentInfoKHR presentInfo{
			.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
			.pNext = VK_NULL_HANDLE,
			.waitSemaphoreCount = 1,
			.pWaitSemaphores = &renderFinishedSemaphores[currentFrame],
			.swapchainCount = 1,
			.pSwapchains = &swapChain,
			.pImageIndices = &imageIndex,
			.pResults = VK_NULL_HANDLE,
		};

		result = vkQueuePresentKHR(presentQueue, &presentInfo);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized) {
			framebufferResized = false;
			recreateSwapChain();
			return;
		} else if (result != VK_SUCCESS) {
			ERROR("Failed to present swap chain image!");
		}

		++currentFrame %= MAX_FRAMES_IN_FLIGHT;
	}

	void recreateSwapChain() {
		int width = 0, height = 0;
		glfwGetFramebufferSize(window, &width, &height);
		if (width == 0 || height == 0) {
			glfwGetFramebufferSize(window, &width, &height);
			LOG("Paused!");
			glfwWaitEvents();
			LOG("Unpaused!");
		}

		vkDeviceWaitIdle(device);

		cleanupSwapchain();

		createSwapChain();
		createImageViews();
		createFramebuffers();
	}

	void mainLoop() {
		LOG("Running main loop");
		while (!glfwWindowShouldClose(window)) {
			glfwPollEvents();
			drawNextFrame();
			if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		}
		vkDeviceWaitIdle(device);
	}

	void cleanupSwapchain() {
		LOG("Destroying image views");
		for (auto imageView : swapChainImageViews) {
			vkDestroyImageView(device, imageView, VK_NULL_HANDLE);
		}

		LOG("Destroying framebuffers");
		for (auto framebuffer : swapChainFramebuffer) {
			vkDestroyFramebuffer(device, framebuffer, VK_NULL_HANDLE);
		}

		LOG("Destroying swap chain");
		vkDestroySwapchainKHR(device, swapChain, VK_NULL_HANDLE);
	}

	void cleanup() {
		LOG("Destroying synchronization objects");
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vkDestroySemaphore(device, imageAvailableSemaphores[i], VK_NULL_HANDLE);
			vkDestroySemaphore(device, renderFinishedSemaphores[i], VK_NULL_HANDLE);
			vkDestroyFence(device, waitFrameFences[i], VK_NULL_HANDLE);
		}

		LOG("Cleaning up swap chain");
		cleanupSwapchain();

		LOG("Cleaning up uniform buffers");
		for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
			vkDestroyBuffer(device, uniformBuffers[i], VK_NULL_HANDLE);
			vkFreeMemory(device, uniformBuffersMemory[i], VK_NULL_HANDLE);
		}

		LOG("Destroying descriptor pool");
		vkDestroyDescriptorPool(device, descriptorPool, VK_NULL_HANDLE);

		LOG("Cleaning up descriptor set layout");
		vkDestroyDescriptorSetLayout(device, descriptorSetLayout, VK_NULL_HANDLE);

		LOG("Destroying vertex buffer");
		vkDestroyBuffer(device, vertexBuffer, VK_NULL_HANDLE);
		vkFreeMemory(device, vertexBufferMemory, VK_NULL_HANDLE);

		LOG("Destroying index buffer");
		vkDestroyBuffer(device, indexBuffer, VK_NULL_HANDLE);
		vkFreeMemory(device, indexBufferMemory, VK_NULL_HANDLE);

		LOG("Destroying command pool");
		vkDestroyCommandPool(device, commandPool, VK_NULL_HANDLE);

		LOG("Destroying graphics pipeline");
		vkDestroyPipeline(device, graphicsPipeline, VK_NULL_HANDLE);

		LOG("Destroying graphics pipeline layout");
		vkDestroyPipelineLayout(device, pipelineLayout, VK_NULL_HANDLE);

		LOG("Destroying render pass");
		vkDestroyRenderPass(device, renderPass, VK_NULL_HANDLE);

		LOG("Destroying window surface");
		vkDestroySurfaceKHR(instance, surface, VK_NULL_HANDLE);

		LOG("Destroying logical device");
		vkDestroyDevice(device, VK_NULL_HANDLE);

		if (enableValidationLayers) {
			LOG("Destroying debug messenger");
			vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, VK_NULL_HANDLE);
		}

		LOG("Destroying Vulkan instance");
		vkDestroyInstance(instance, VK_NULL_HANDLE);

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
