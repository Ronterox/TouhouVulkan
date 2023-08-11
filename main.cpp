#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <iostream>
#include <stdexcept>
#include <cstdlib>

#define __FILE_LINE__ __FILE__ << ":" << __LINE__ << ":"

#define LOG(x) std::cout << __FILE_LINE__ << x << std::endl
#define LOGE(x) std::cerr << __FILE_LINE__ << x << std::endl

#define ERROR(x) throw std::runtime_error(x)

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

    GLFWwindow *window;
    VkInstance instance;

    void initWindow()
    {
        if (!glfwInit())
            LOGE("GLFW Initialization Error!");

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        LOG("Initializing windows");
        window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

        if (!window)
            LOGE("Window creation error!");
        else
            LOG("Window initialized!");
    }

    void initVulkan()
    {
        createInstance();
    }

    void mainLoop()
    {
        while (!glfwWindowShouldClose(window))
            glfwPollEvents();
    }

    // This is optional, but it helps the driver optimize for our application
    void createInstance()
    {
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

        uint32_t glfwExtensionCount = 0;
        const char **glfwExtensions;

        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0;

        LOG("Extensions enabled");

        if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS)
            ERROR("Failed to create instance!");
        else
            LOG("Instance created!");
    }

    void cleanup()
    {
        LOG("Cleaning up");

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
    catch (const std::exception &e)
    {
        LOGE(e.what());
        return EXIT_FAILURE;
    }
}