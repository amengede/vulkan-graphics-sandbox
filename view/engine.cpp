#include "engine.h"
#include "vkInit/instance.h"
#include "vkInit/device.h"
#include "vkInit/swapchain.h"
#include "vkInit/commands.h"
#include "vkInit/sync.h"
#include "graphics_library.h"

Engine::Engine(int width, int height, GLFWwindow* window) {

	this->width = width;
	this->height = height;
	this->window = window;

	vkLogging::Logger::get_logger()->print("Making a graphics engine...");

	make_instance();

	make_device();

	finalize_setup();

}

void Engine::make_instance() {

	instance = vkInit::make_instance("ID Tech 12");
	dldi = vk::DispatchLoaderDynamic(instance, vkGetInstanceProcAddr);
	if (vkLogging::Logger::get_logger()->get_debug_mode()) {
		debugMessenger = vkLogging::make_debug_messenger(instance, dldi);
	}
	VkSurfaceKHR c_style_surface;
	if (glfwCreateWindowSurface(instance, window, nullptr, &c_style_surface) != VK_SUCCESS) {
		vkLogging::Logger::get_logger()->print("Failed to abstract glfw surface for Vulkan.");
	}
	else {
		vkLogging::Logger::get_logger()->print(
			"Successfully abstracted glfw surface for Vulkan.");
	}
	//copy constructor converts to hpp convention
	surface = c_style_surface;
}

void Engine::make_device() {

	physicalDevice = vkInit::choose_physical_device(instance);
	device = vkInit::create_logical_device(physicalDevice, surface);
	std::array<vk::Queue,2> queues = vkInit::get_queues(physicalDevice, device, surface);
	graphicsQueue = queues[0];
	presentQueue = queues[1];
	make_swapchain();
	frameNumber = 0;
}

/**
* Make a swapchain
*/
void Engine::make_swapchain() {

	vkInit::SwapChainBundle bundle = vkInit::create_swapchain(
		device, physicalDevice, surface, width, height
	);
	swapchain = bundle.swapchain;
	swapchainFrames = bundle.frames;
	swapchainFormat = bundle.format;
	swapchainExtent = bundle.extent;
	maxFramesInFlight = static_cast<int>(swapchainFrames.size());
	choose_color_conversion_function();

	//std::cout << "Format: " << (int)swapchainFormat << std::endl;

	for (vkUtil::SwapChainFrame& frame : swapchainFrames) {
		frame.logicalDevice = device;
		frame.physicalDevice = physicalDevice;
		frame.width = swapchainExtent.width;
		frame.height = swapchainExtent.height;
	}

}

void Engine::choose_color_conversion_function() {

	if (swapchainFormat == vk::Format::eR8G8B8A8Unorm) {
		convert_color = &convert_to_r8g8b8a8_unorm;
	}

	else if (swapchainFormat == vk::Format::eB8G8R8A8Unorm) {
		convert_color = &convert_to_b8g8r8a8_unorm;
	}
}

void Engine::clear_screen(float r, float g, float b) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	for (int i = 0; i < _frame.width * _frame.height; ++i) {
		_frame.colorBufferData[4 * i]     = color[0];
		_frame.colorBufferData[4 * i + 1] = color[1];
		_frame.colorBufferData[4 * i + 2] = color[2];
		_frame.colorBufferData[4 * i + 3] = color[3];
	}
}

void Engine::clear_screen_avx2(float r, float g, float b) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	__m256 block = _mm256_set1_ps(*(float*)color);

	int pixelCount = _frame.width * _frame.height;
	int blockCount = pixelCount / 8;

	__m256* blocks = (__m256*) _frame.colorBufferData.data();

	for (int i = 0; i < blockCount; ++i) {
		blocks[i] = block;
	}
	
	for (int i = 8 * blockCount; i < pixelCount; ++i) {
		_frame.colorBufferData[4 * i] = color[0];
		_frame.colorBufferData[4 * i + 1] = color[1];
		_frame.colorBufferData[4 * i + 2] = color[2];
		_frame.colorBufferData[4 * i + 3] = color[3];
	}
	
}

/**
* The swapchain must be recreated upon resize or minimization, among other cases
*/
void Engine::recreate_swapchain() {

	width = 0;
	height = 0;
	while (width == 0 || height == 0) {
		glfwGetFramebufferSize(window, &width, &height);
		glfwWaitEvents();
	}

	device.waitIdle();

	cleanup_swapchain();
	make_swapchain();
	make_frame_resources();
	vkInit::commandBufferInputChunk commandBufferInput = { device, commandPool, swapchainFrames };
	vkInit::make_frame_command_buffers(commandBufferInput);

}

void Engine::finalize_setup() {

	commandPool = vkInit::make_command_pool(device, physicalDevice, surface);

	vkInit::commandBufferInputChunk commandBufferInput = { device, commandPool, swapchainFrames };
	mainCommandBuffer = vkInit::make_command_buffer(commandBufferInput);
	vkInit::make_frame_command_buffers(commandBufferInput);

	make_frame_resources();

}

void Engine::make_frame_resources() {

	for (vkUtil::SwapChainFrame& frame : swapchainFrames) {

		frame.imageAvailable = vkInit::make_semaphore(device);
		frame.renderFinished = vkInit::make_semaphore(device);
		frame.inFlight = vkInit::make_fence(device);

		frame.setup();
	}

}

void Engine::flush_frame(uint32_t imageIndex, uint32_t frameNumber) {

	vk::CommandBuffer& commandBuffer = swapchainFrames[frameNumber].commandBuffer;

	vk::CommandBufferBeginInfo beginInfo = {};

	try {
		commandBuffer.begin(beginInfo);
	}
	catch (vk::SystemError err) {
		vkLogging::Logger::get_logger()->print("Failed to begin recording command buffer!");
	}

	swapchainFrames[imageIndex].flush();

	try {
		commandBuffer.end();
	}
	catch (vk::SystemError err) {
		
		vkLogging::Logger::get_logger()->print("failed to record command buffer!");
	}
}

void Engine::render() {

	device.waitForFences(1, &(swapchainFrames[frameNumber].inFlight), VK_TRUE, UINT64_MAX);
	device.resetFences(1, &(swapchainFrames[frameNumber].inFlight));

	uint32_t imageIndex;
	try {
		vk::ResultValue acquire = device.acquireNextImageKHR(
			swapchain, UINT64_MAX, 
			swapchainFrames[frameNumber].imageAvailable, nullptr
		);
		imageIndex = acquire.value;
	}
	catch (vk::OutOfDateKHRError error) {
		std::cout << "Recreate" << std::endl;
		recreate_swapchain();
		return;
	}
	catch (vk::IncompatibleDisplayKHRError error) {
		std::cout << "Recreate" << std::endl;
		recreate_swapchain();
		return;
	}
	catch (vk::SystemError error) {
		std::cout << "Failed to acquire swapchain image!" << std::endl;
	}

	vk::CommandBuffer& commandBuffer = swapchainFrames[frameNumber].commandBuffer;

	commandBuffer.reset();

	flush_frame(imageIndex, frameNumber);

	vk::SubmitInfo submitInfo = {};

	vk::Semaphore waitSemaphores[] = { swapchainFrames[frameNumber].imageAvailable };
	vk::PipelineStageFlags waitStages[] = { vk::PipelineStageFlagBits::eColorAttachmentOutput };
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = waitSemaphores;
	submitInfo.pWaitDstStageMask = waitStages;

	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;

	vk::Semaphore signalSemaphores[] = { swapchainFrames[frameNumber].renderFinished };
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = signalSemaphores;

	try {
		graphicsQueue.submit(submitInfo, swapchainFrames[frameNumber].inFlight);
	}
	catch (vk::SystemError err) {
		vkLogging::Logger::get_logger()->print("failed to submit draw command buffer!");
	}

	vk::PresentInfoKHR presentInfo = {};
	presentInfo.waitSemaphoreCount = 1;
	presentInfo.pWaitSemaphores = signalSemaphores;

	vk::SwapchainKHR swapChains[] = { swapchain };
	presentInfo.swapchainCount = 1;
	presentInfo.pSwapchains = swapChains;

	presentInfo.pImageIndices = &imageIndex;

	vk::Result present;

	try {
		present = presentQueue.presentKHR(presentInfo);
	}
	catch (vk::OutOfDateKHRError error) {
		present = vk::Result::eErrorOutOfDateKHR;
	}

	if (present == vk::Result::eErrorOutOfDateKHR || present == vk::Result::eSuboptimalKHR) {
		std::cout << "Recreate" << std::endl;
		recreate_swapchain();
		return;
	}

	frameNumber = (frameNumber + 1) % maxFramesInFlight;

}

/**
* Free the memory associated with the swapchain objects
*/
void Engine::cleanup_swapchain() {

	for (vkUtil::SwapChainFrame& frame : swapchainFrames) {
		frame.destroy();
	}
	device.destroySwapchainKHR(swapchain);

}

Engine::~Engine() {

	device.waitIdle();

	vkLogging::Logger::get_logger()->print("Goodbye see you!");

	device.destroyCommandPool(commandPool);

	cleanup_swapchain();

	device.destroy();

	instance.destroySurfaceKHR(surface);
	if (vkLogging::Logger::get_logger()->get_debug_mode()) {
		instance.destroyDebugUtilsMessengerEXT(debugMessenger, nullptr, dldi);
	}
	/*
	* from vulkan_funcs.hpp:
	* 
	* void Instance::destroy( Optional<const VULKAN_HPP_NAMESPACE::AllocationCallbacks> allocator = nullptr,
                                            Dispatch const & d = ::vk::getDispatchLoaderStatic())
	*/
	instance.destroy();

	//terminate glfw
	glfwTerminate();
}