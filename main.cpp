#define GLFW_INCLUDE_VULKAN
// #define NDEBUG
#include <GLFW/glfw3.h>

#include <iostream>
#include <vector>
#include <algorithm>
#include <cstring>

#define __FILE_LINE__ __FILE__ << ":" << __LINE__ << ":"

#define LOG(x) std::cout << __FILE_LINE__ << x << std::endl
#define LOGE(x) std::cerr << __FILE_LINE__ << x << std::endl
#define ERROR(x) throw std::runtime_error(x)

template<typename T>
using list = std::vector<T>;

class HelloTriangleApplication
{
public:
    void run()
    {
        initWindow();
        initVulkan();
        mainLoop();
        cleanup();
    }

private:
    const uint32_t WIDTH = 800;
    const uint32_t HEIGHT = 600;

    const list<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

#ifdef NDEBUG
    const bool enableValidationLayers = false;
#else
    const bool enableValidationLayers = true;
#endif

    GLFWwindow* window;
    VkInstance instance;

    void initWindow()
    {
        if (!glfwInit())
            LOGE("GLFW Initialization Error!");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        LOG("Initializing windows");
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        if (!window) LOGE("Window creation error!");
        else LOG("Window initialized!");
    }

    void initVulkan()
    {
        createInstance();
    }

    void mainLoop()
    {
        glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) glfwSetWindowShouldClose(window, GLFW_TRUE);
        });

        while (!glfwWindowShouldClose(window)) glfwPollEvents();
    }

    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {

        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

        return VK_FALSE;
    }

    // Remember to free memory, LMAO
    list<VkExtensionProperties>* getAvailableExtensions()
    {
        uint32_t extensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

        auto* extensions = new std::vector<VkExtensionProperties>(extensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions->data());

        return extensions;
    }

    bool checkValidationLayerSupport() {
        uint32_t layerCount = 0;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

        for (const char* layerName : validationLayers) {
            const auto layerFound = [layerName](VkLayerProperties& props) {
                return strcmp(props.layerName, layerName) == 0;
            };
            if (!std::any_of(availableLayers.begin(), availableLayers.end(), layerFound)) return false;
        }

        return true;
    }

    list<const char*> getRequiredExtensions() {
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        list<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

        if (enableValidationLayers) extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);

        return extensions;
    }

    // This is optional, but it helps the driver optimize for our application
    void createInstance()
    {
        if (enableValidationLayers && !checkValidationLayerSupport()) ERROR("Validation layers requested, but not available!");

        LOG("Creating instance");
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_API_VERSION_1_0;

        LOG("Application info created");

        VkInstanceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &appInfo;

        LOG("Info created");

        list<const char*> requiredExtensions = getRequiredExtensions();
        uint32_t extensionCount = static_cast<uint32_t>(requiredExtensions.size());

        createInfo.enabledExtensionCount = extensionCount;
        createInfo.ppEnabledExtensionNames = requiredExtensions.data();

        if (enableValidationLayers) {
            createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
            createInfo.ppEnabledLayerNames = validationLayers.data();
        }
        else
            createInfo.enabledLayerCount = 0;

        LOG("Checking for extensions");
        list<VkExtensionProperties>* availableExtensions = getAvailableExtensions();

        const list<VkExtensionProperties>::iterator start = availableExtensions->begin(), end = availableExtensions->end();
        for (int i = 0; i < extensionCount; ++i)
        {
            const char* extension = requiredExtensions[i];

            const auto includesExtension = [extension](VkExtensionProperties& props) {
                return strcmp(props.extensionName, extension) == 0;
            };

            if (!std::any_of(start, end, includesExtension)) {
                LOGE("Extension " << extension << " is not available");
                break;
            }
        }
        delete availableExtensions;

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            ERROR("Failed to create instance!");
        else
            LOG("Instance created!");
    }

    void cleanup()
    {
        LOG("Cleaning up");

        vkDestroyInstance(instance, nullptr);
        LOG("Instance destroyed!");

        glfwDestroyWindow(window);
        LOG("Window destroyed");

        glfwTerminate();
        LOG("GLFW terminated");
    }
};

int main()
{
    HelloTriangleApplication app;

    try
    {
        app.run();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& e)
    {
        LOGE(e.what());
        return EXIT_FAILURE;
    }
}