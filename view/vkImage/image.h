#pragma once
#include "../../config.h"

typedef struct {
	float* r;
	float* g;
	float* b;
	int width, height;
} texture;

namespace vkImage {

	/**
		Create a view of a vulkan image.
	*/
	vk::ImageView make_image_view(
		vk::Device logicalDevice, vk::Image image, vk::Format format,
		vk::ImageAspectFlags aspect);
}