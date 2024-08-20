void createSyncObjects() {
	createFences();
	createSemaphores();
}

void createBuffer(buffer) {
	createStagingBuffer(stagingBuffer);
	mapDataFrom(stagingBuffer, buffer);
	destroyStagingBuffer(stagingBuffer);
}

void commandBufferRequestDraw() {
	waitFence();
	createBuffer(verticesBuffer);
	createBuffer(indexBuffer);
	copyDataFrom(indexBuffer, veticesBuffer);
	startCommandBuffer();
	drawRequestVertices();
	stopCommandBuffer();
}

void draw() {
	while (win.event) {
		commandBufferRequestDraw();
	}
}

int main() {
	initVulkan();
	initWindow();

	listPhysicalDevices();
	selectPhysicalDevice();
	selectLogicalDevice();

	createImages();
	createImageViews();

	createFramebuffer();
	createBufferView();

	createCommandPool();
	createCommandBuffer();

	createSyncObjects();
	createVerticesBuffer();
	createIndexBuffer();

	createRenderPass();

	draw();

	cleanup();
}
