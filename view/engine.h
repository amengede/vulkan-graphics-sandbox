#pragma once
#include "../config.h"
#include "vkUtil/frame.h"
#include "vkImage/image.h"
#include "../linear_algebros.h"

class Engine {

public:

	Engine(int width, int height, GLFWwindow* window);

	~Engine();

	void clear_screen(float r, float g, float b);

	void clear_screen_avx2(float r, float g, float b);

	void draw_horizontal_line(float r, float g, float b, int x1, int x2, int y);

	void draw_horizontal_line_avx2(float r, float g, float b, int x1, int x2, int y);

	void draw_vertical_line(float r, float g, float b, int x, int y1, int y2);

	void draw_line_naive(float r, float g, float b, int x1, int y1, int x2, int y2);

	void draw_shallow_line_naive(float r, float g, float b, int x1, int y1, int x2, int y2);

	void draw_steep_line_naive(float r, float g, float b, int x1, int y1, int x2, int y2);

	void draw_line_bresenham(float r, float g, float b, int x1, int y1, int x2, int y2);

	void draw_shallow_line_bresenham(float r, float g, float b, int x1, int y1, int x2, int y2);

	void draw_steep_line_bresenham(float r, float g, float b, int x1, int y1, int x2, int y2);

	void draw_polygon_flat(float r, float g, float b, edgeTable polygon);

	void trace_shallow_edge(int x1, int y1, int x2, int y2, int* x_start, int* x_end);

	void trace_steep_edge(int x1, int y1, int x2, int y2, int* x_start, int* x_end);

	void render();

private:

	//glfw-related variables
	int width;
	int height;
	GLFWwindow* window;

	//instance-related variables
	vk::Instance instance{ nullptr };
	vk::DebugUtilsMessengerEXT debugMessenger{ nullptr };
	vk::DispatchLoaderDynamic dldi;
	vk::SurfaceKHR surface;

	//device-related variables
	vk::PhysicalDevice physicalDevice{ nullptr };
	vk::Device device{ nullptr };
	vk::Queue graphicsQueue{ nullptr };
	vk::Queue presentQueue{ nullptr };
	vk::SwapchainKHR swapchain{ nullptr };
	std::vector<vkUtil::SwapChainFrame> swapchainFrames;
	vk::Format swapchainFormat;
	vk::Extent2D swapchainExtent;

	//Command-related variables
	vk::CommandPool commandPool;
	vk::CommandBuffer mainCommandBuffer;

	//Synchronization objects
	int maxFramesInFlight, frameNumber;

	//Color conversion function
	unsigned char* (*convert_color)(float, float, float);

	//instance setup
	void make_instance();

	//device setup
	void make_device();
	void make_swapchain();
	void recreate_swapchain();

	//final setup steps
	void finalize_setup();
	void make_frame_resources();

	void flush_frame(uint32_t imageIndex, uint32_t frameNumber);

	void choose_color_conversion_function();

	//Cleanup functions
	void cleanup_swapchain();
};