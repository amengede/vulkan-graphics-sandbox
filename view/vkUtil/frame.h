#pragma once
#include "../../config.h"

namespace vkUtil {

	/**
		Holds the data structures associated with a "Frame"
	*/
	class SwapChainFrame {

	public:

		//For doing work
		vk::Device logicalDevice;
		vk::PhysicalDevice physicalDevice;

		//Swapchain-type stuff
		vk::Image image;
		vk::ImageView imageView;
		int width, height;

		vk::CommandBuffer commandBuffer;

		//Sync objects
		vk::Semaphore imageAvailable, renderFinished;
		vk::Fence inFlight;

		//Resources
		std::vector<unsigned char> colorBufferData;

		//Staging Buffer
		Buffer stagingBuffer;
		void* writeLocation;

		//Transition Jobs
		vk::ImageSubresourceRange colorBufferAccess;
		vk::ImageMemoryBarrier barrier, barrier2;

		//Copy Job
		vk::BufferImageCopy copy;
		vk::ImageSubresourceLayers copyAccess;

		void setup();

		void flush();

		void destroy();
	};

}