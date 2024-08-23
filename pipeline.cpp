
void initWindow() {
	glfwInitStuff();
	glfwAddResizeHint();
	glfwRemoveAPIOfWindow();
	glfwHintResizeCallback();
}

void windowSurface() { glfwWinSurfaceGet(); }

void pickPhysicalDevice() {
	listDevices();
	getScores();
	pickSuitableGPU();
}

void createLogicalDevice() {
	pickGraphicsQueueFamily();
	pickPresentQueueFamily();
	vkCreateLogicalDevice();

	// For submitting later
	vkGetPresentQueue(presentQueue);
	vkGetGraphicsQueue(graphicsQueue);
}

void createSwapChain() {
	auto surfaceFormat = getSurfaceFormat();
	auto presentMode = getPresentMode();
	auto extent = getExtent();
	vkCreateSwapchain(device, surface);
	vkGetImagesSwapchain(images);
}

void createRenderPass() {
	fillRenderPassData();
	createSubpass();
}

void createImageViews() {
	auto images = getImagesFromSwapchain();
	for (auto img : images) {
		vkCreateImageView();
	}
}

void createGraphicsPipeline() {
	auto shaderf = loadShader(path);
	auto shaderv = loadShader(path);

	auto shaderFrag = createShaderModule(shaderf);
	auto shaderVert = createShaderModule(shaderv);

	vkCreateGraphicsPipelines();
}

void createFramebuffer() {
	for (auto imageView : images) {
		vkCreateFramebuffer(imageView, framebuffers[i]);
	}
}

void createSyncObjects() {
	auto fence[];
	vkCreateFence(fence);

	auto createSemaphore;
	vkCreateSemaphore();

	auto presentSemaphore;
	vkCreateSemaphore();
}

void createCommandPool() {
	auto graphicsFamily = getQueueFamily();
	vkCreateCommandPool(graphicsFamily);
}

void createCommandBuffer() { vkCreateCommandBuffer(); }

void createAndAllocate() {}

void createVertexBuffer() { createAndAllocate(); }

vodi createIndexBuffer() { createAndAllocate(); }

void initVulkan() {
	vkCreateVulkanInstance();
	createDebugMessageValidation();

	windowSurface();
	pickPhysicalDevice();
	createLogicalDevice();

	createSwapChain();
	createImageViews();

	createRenderPass();
	createGraphicsPipeline();

	createFramebuffer();
	createCommandPool();
	createCommandBuffer();

	createVertexBuffer();
	createIndexBuffer();

	createSyncObjects();
}

void drawNextFrame() {
	waitFences(fences[i]);

	auto result = vkGetFrame(createSemaphore);
	checkForResize(result);

	vkClearFence(fences[i]);

	auto result = vkPresentFrame(presentSemaphore);

	i++ % #fences;
}

void mainLoop() {
	while (glfwWindowShouldClose()) {
		glfwPollEvents();
		drawNextFrame();

		if (check == Escape) {
			setShouldClose(true);
		}
	}
}

void main() {
	initWindow();
	initVulkan();
	mainLoop();
	cleanup();
}
