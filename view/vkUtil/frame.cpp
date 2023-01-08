#include "frame.h"
#include "memory.h"

void vkUtil::SwapChainFrame::setup() {

	colorBufferData.reserve(4 * width * height);

	for (int i = 0; i < width * height; ++i) {
		colorBufferData.push_back(0xff);
		colorBufferData.push_back(0x00);
		colorBufferData.push_back(0x00);
		colorBufferData.push_back(0x00);
	}

	BufferInputChunk input;
	input.logicalDevice = logicalDevice;
	input.physicalDevice = physicalDevice;
	input.memoryProperties = vk::MemoryPropertyFlagBits::eHostCoherent | vk::MemoryPropertyFlagBits::eHostVisible;
	input.usage = vk::BufferUsageFlagBits::eTransferSrc;
	input.size = width * height * 4;

	stagingBuffer = vkUtil::createBuffer(input);

	writeLocation = logicalDevice.mapMemory(stagingBuffer.bufferMemory, 0, input.size);

	colorBufferAccess.aspectMask = vk::ImageAspectFlagBits::eColor;
	colorBufferAccess.baseMipLevel = 0;
	colorBufferAccess.levelCount = 1;
	colorBufferAccess.baseArrayLayer = 0;
	colorBufferAccess.layerCount = 1;

	barrier.oldLayout = vk::ImageLayout::eUndefined;
	barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier.image = image;
	barrier.subresourceRange = colorBufferAccess;
	barrier.srcAccessMask = vk::AccessFlagBits::eNoneKHR;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

	copy.bufferOffset = 0;
	copy.bufferRowLength = 0;
	copy.bufferImageHeight = 0;

	copyAccess.aspectMask = vk::ImageAspectFlagBits::eColor;
	copyAccess.mipLevel = 0;
	copyAccess.baseArrayLayer = 0;
	copyAccess.layerCount = 1;
	copy.imageSubresource = copyAccess;

	copy.imageOffset = vk::Offset3D(0, 0, 0);
	copy.imageExtent = vk::Extent3D(width,height,1);

	barrier2.oldLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier2.newLayout = vk::ImageLayout::ePresentSrcKHR;
	barrier2.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier2.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	barrier2.image = image;
	barrier2.subresourceRange = colorBufferAccess;
	barrier2.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier2.dstAccessMask = vk::AccessFlagBits::eNoneKHR;

}

void vkUtil::SwapChainFrame::flush() {

	memcpy(writeLocation, colorBufferData.data(), width * height * 4);

	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTopOfPipe, 
		vk::PipelineStageFlagBits::eTransfer, 
		vk::DependencyFlags(), nullptr, nullptr, barrier);

	commandBuffer.copyBufferToImage(
		stagingBuffer.buffer, image, vk::ImageLayout::eTransferDstOptimal, copy
	);

	commandBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eBottomOfPipe,
		vk::DependencyFlags(), nullptr, nullptr, barrier2);

}

void vkUtil::SwapChainFrame::destroy() {

	logicalDevice.unmapMemory(stagingBuffer.bufferMemory);
	logicalDevice.freeMemory(stagingBuffer.bufferMemory);
	logicalDevice.destroyBuffer(stagingBuffer.buffer);

	logicalDevice.destroyImageView(imageView);
	logicalDevice.destroyFence(inFlight);
	logicalDevice.destroySemaphore(imageAvailable);
	logicalDevice.destroySemaphore(renderFinished);

}