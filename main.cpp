#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <set>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "utils.h"

const int WIDTH = 800;
const int HEIGHT = 600;

const list<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
#ifdef NDEBUG
const bool enableValidationLayers = false;
#else
const bool enableValidationLayers = true;
#endif

const list<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

struct QueueFamilyIndices {
	std::optional<uint32_t> graphicsFamily, presentFamily;

	bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
};

struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities;
	list<VkSurfaceFormatKHR> formats;
	list<VkPresentModeKHR> presentModes;
};

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
	VkPipelineLayout pipelineLayout;

	VkSurfaceKHR surface;
	VkSwapchainKHR swapChain;
	VkFormat swapChainImageFormat;
	VkExtent2D swapChainExtent;

	list<VkImage> swapChainImages;
	list<VkImageView> swapChainImageViews;

	VkQueue graphicsQueue, presentQueue;
	VkDevice device;

	VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
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
							 [&](const auto &ext) { return IS_STR_EQUAL(ext.extensionName, glfwRequiredEXT[i]); })) {
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

	void pickPhysicalDevice() {
		uint32_t physicalDeviceCount = 0;
		vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr);

		list<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
		vkEnumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data());

		if (physicalDeviceCount == 0) {
			ERROR("Failed to find GPUs with Vulkan support!");
		}

		std::multimap<int, VkPhysicalDevice> candidates;
		for (const auto &device : physicalDevices) {
			int score = getDeviceScore(device);
			candidates.insert(std::make_pair(score, device));
		}

		LOG(candidates.size() << " GPUs found");
		if (candidates.rbegin()->first > 0) {
			physicalDevice = candidates.rbegin()->second;
		} else {
			ERROR("Failed to find a suitable GPU!");
		}

		if (!isDeviceSuitable(physicalDevice)) {
			ERROR("GPU is not suitable");
		}

		LOG("GPU successfully selected with a score of " << candidates.rbegin()->first);
	}

	QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device) {
		uint32_t queueFamilyCount = 0;
		vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

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

	bool isDeviceSuitable(VkPhysicalDevice device) {
		QueueFamilyIndices indices = findQueueFamilies(device);
		bool extensionsSupported = checkDeviceExtensionSupport(device);
		bool swapChainAdequate = false;
		if (extensionsSupported) {
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
			swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
		}
		return swapChainAdequate && indices.isComplete();
	}

	bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
		uint32_t extensionCount;
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

		list<VkExtensionProperties> availableExtensions(extensionCount);
		vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

		for (const auto &ext : deviceExtensions) {
			if (std::none_of(availableExtensions.begin(), availableExtensions.end(),
							 [&](const auto &extension) { return IS_STR_EQUAL(extension.extensionName, ext); })) {
				return false;
			}
		}
		return true;
	}

	int getDeviceScore(VkPhysicalDevice device) {
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
		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		list<VkDeviceQueueCreateInfo> queueCreateInfos;
		std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()};

		float queuePriority = 1.0f;
		for (uint32_t queueFamily : uniqueQueueFamilies) {
			VkDeviceQueueCreateInfo queueCreateInfo{
				.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
				.queueFamilyIndex = queueFamily,
				.queueCount = 1,
				.pQueuePriorities = &queuePriority,
			};
			queueCreateInfos.push_back(queueCreateInfo);
		}

		VkDeviceCreateInfo createInfo{
			.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
			.pQueueCreateInfos = queueCreateInfos.data(),
			.enabledLayerCount = 0,
			.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size()),
			.ppEnabledExtensionNames = deviceExtensions.data(),
		};

		if (enableValidationLayers) {
			createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
			createInfo.ppEnabledLayerNames = validationLayers.data();
		}

		if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
			ERROR("Failed to create logical device!");
		}

		LOG("Obtaining graphicsFamily queue");
		vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);

		LOG("Obtaining presentFamily queue");
		vkGetDeviceQueue(device, indices.presentFamily.value(), 0, &presentQueue);

		LOG("Logical device created");
	}

	void createWindowSurface() {
		LOG("Creating window surface");
		if (glfwCreateWindowSurface(instance, window, nullptr, &surface) != VK_SUCCESS) {
			ERROR("Failed to create window surface!");
		}
		LOG("Window surface created");
	}

	SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device) {
		SwapChainSupportDetails details;
		vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

		uint32_t formatCount;
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

		if (formatCount != 0) {
			details.formats.resize(formatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
		}

		uint32_t presentModeCount;
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

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
		SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

		LOG("Choosing swap chain details");
		VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
		VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
		VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

		VkSwapchainCreateInfoKHR createInfo{
			.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			.surface = surface,
			.minImageCount = swapChainSupport.capabilities.minImageCount + 1,
			.imageFormat = surfaceFormat.format,
			.imageColorSpace = surfaceFormat.colorSpace,
			.imageExtent = extent,
			.imageArrayLayers = 1,
			.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,

			.preTransform = swapChainSupport.capabilities.currentTransform,
			.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,

			.presentMode = presentMode,
			.clipped = VK_TRUE,

			.oldSwapchain = VK_NULL_HANDLE,
		};

		QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

		if (indices.graphicsFamily != indices.presentFamily) {
			uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};
			createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			createInfo.queueFamilyIndexCount = 2;
			createInfo.pQueueFamilyIndices = queueFamilyIndices;
		} else {
			createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		}

		LOG("Creating swap chain");
		if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
			ERROR("Failed to create swap chain!");
		}

		LOG("Swap chain created");
		swapChainImageFormat = surfaceFormat.format;
		swapChainExtent = extent;

		LOG("Obtaining swap chain images");
		uint32_t imageCount;
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);

		swapChainImages.resize(imageCount);
		vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

		LOG("Swap chain images obtained");
	}

	void createImageViews() {
		swapChainImageViews.resize(swapChainImages.size());

		for (size_t i = 0; i < swapChainImages.size(); ++i) {
			VkImageViewCreateInfo createInfo{
				.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
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

			if (vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]) != VK_SUCCESS) {
				ERROR("Failed to create image views!");
			}
		}
	}

	void createGraphicsPipeline() {
		LOG("Initializing graphics pipeline creation");

		// TODO: Add shader and vertex modules here

		list<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH};

		VkPipelineDynamicStateCreateInfo dynamicStateCreateInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()),
			.pDynamicStates = dynamicStates.data(),
		};

		VkPipelineVertexInputStateCreateInfo vertexInputInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
			.vertexBindingDescriptionCount = 0,
			.vertexAttributeDescriptionCount = 0,
		};

		VkPipelineInputAssemblyStateCreateInfo inputAssembly{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE,
		};

		VkPipelineViewportStateCreateInfo viewportState{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1,
			.scissorCount = 1,
		};

		VkPipelineRasterizationStateCreateInfo rasterizer{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE,

			.polygonMode = VK_POLYGON_MODE_FILL,
			.cullMode = VK_CULL_MODE_BACK_BIT,
			.frontFace = VK_FRONT_FACE_CLOCKWISE,

			.depthBiasEnable = VK_FALSE,
			.lineWidth = 1.0f,
		};

		VkPipelineMultisampleStateCreateInfo multisampling{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable = VK_FALSE,
		};

		// finalColor.rgb = newColor.rgb * newColor.a + oldColor.rgb * (1 - newColor.a)
		// finalColor.a = newColor.a * 1 + oldColor.a * 0
		VkPipelineColorBlendAttachmentState colorBlendAttachment{
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

		VkPipelineColorBlendStateCreateInfo colorBlending{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable = VK_FALSE,
			.attachmentCount = 1,
			.pAttachments = &colorBlendAttachment,
		};

		VkPipelineLayoutCreateInfo pipelineLayoutInfo{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
		};

		LOG("Creating graphics pipeline layout");
		if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
			ERROR("Failed to create pipeline layout!");
		}

		LOG("Pipeline layout created");
	}

	void initVulkan() {
		createVkInstance();
		setupDebugMessenger();
		createWindowSurface();
		pickPhysicalDevice();
		createLogicalDevice();
		createSwapChain();
		createImageViews();
		createGraphicsPipeline();
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
		LOG("Destroying graphics pipeline layout");
		vkDestroyPipelineLayout(device, pipelineLayout, nullptr);

		LOG("Destroying image views");
		for (auto imageView : swapChainImageViews) {
			vkDestroyImageView(device, imageView, nullptr);
		}

		LOG("Destroying swap chain");
		vkDestroySwapchainKHR(device, swapChain, nullptr);

		LOG("Destroying window surface");
		vkDestroySurfaceKHR(instance, surface, nullptr);

		LOG("Destroying logical device");
		vkDestroyDevice(device, nullptr);

		if (enableValidationLayers) {
			LOG("Destroying debug messenger");
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
