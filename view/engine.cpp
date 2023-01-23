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

void Engine::draw_horizontal_line(float r, float g, float b, int x1, int x2, int y) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	x1 = std::min(_frame.width - 1, std::max(0, x1));
	x2 = std::min(_frame.width - 1, std::max(0, x2));
	y = std::min(_frame.height - 1, std::max(0, y));

	for (int x = x1; x < x2; ++x) {
		int pixel = 4 * (_frame.width * y + x);
		_frame.colorBufferData[pixel] = color[0];
		_frame.colorBufferData[pixel + 1] = color[1];
		_frame.colorBufferData[pixel + 2] = color[2];
		_frame.colorBufferData[pixel + 3] = color[3];
	}
}

void Engine::draw_horizontal_line_avx2(float r, float g, float b, int x1, int x2, int y) {

	if ((x2 - x1) < 16) {
		draw_horizontal_line(r, g, b, x1, x2, y);
		return;
	}

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	x1 = std::min(_frame.width - 1, std::max(0, x1));
	x2 = std::min(_frame.width - 1, std::max(0, x2));
	y = std::min(_frame.height - 1, std::max(0, y));

	__m256 block = _mm256_set1_ps(*(float*)color);

	int startPixel = _frame.width * y + x1;
	int startBlock = (startPixel / 8) + 1;
	int endPixel = _frame.width * y + x2;
	int endBlock = (endPixel / 8) - 1;

	__m256* blocks = (__m256*) _frame.colorBufferData.data();

	for (int pixel = startPixel; pixel < startBlock * 8; ++pixel) {
		_frame.colorBufferData[4 * pixel] = color[0];
		_frame.colorBufferData[4 * pixel + 1] = color[1];
		_frame.colorBufferData[4 * pixel + 2] = color[2];
		_frame.colorBufferData[4 * pixel + 3] = color[3];
	}

	for (int i = startBlock; i < endBlock; ++i) {
		blocks[i] = block;
	}

	for (int pixel = endBlock * 8; pixel < endPixel; ++pixel) {
		_frame.colorBufferData[4 * pixel] = color[0];
		_frame.colorBufferData[4 * pixel + 1] = color[1];
		_frame.colorBufferData[4 * pixel + 2] = color[2];
		_frame.colorBufferData[4 * pixel + 3] = color[3];
	}
}

void Engine::draw_vertical_line(float r, float g, float b, int x, int y1, int y2) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	x = std::min(_frame.width - 1, std::max(0, x));
	y1 = std::min(_frame.height - 1, std::max(0, y1));
	y2 = std::min(_frame.height - 1, std::max(0, y2));

	for (int y = y1; y < y2; ++y) {
		int pixel = 4 * (_frame.width * y + x);
		_frame.colorBufferData[pixel] = color[0];
		_frame.colorBufferData[pixel + 1] = color[1];
		_frame.colorBufferData[pixel + 2] = color[2];
		_frame.colorBufferData[pixel + 3] = color[3];
	}
}

void Engine::draw_line_naive(float r, float g, float b, int x1, int y1, int x2, int y2) {

	if (x1 == x2) {
		if (y1 < y2) {
			draw_vertical_line(r, g, b, x1, y1, y2);
		}
		else {
			draw_vertical_line(r, g, b, x1, y2, y1);
		}
		return;
	}

	if (y1 == y2) {
		if (x1 < x2) {
			draw_horizontal_line_avx2(r, g, b, x1, x2, y1);
		}
		else {
			draw_horizontal_line_avx2(r, g, b, x2, x1, y1);
		}
		return;
	}

	if (abs(y2 - y1) < abs(x2 - x1)) {
		if (x1 < x2) {
			draw_shallow_line_naive(r, g, b, x1, y1, x2, y2);
		}
		else {
			draw_shallow_line_naive(r, g, b, x2, y2, x1, y1);
		}
		return;
	}

	if (y1 < y2) {
		draw_steep_line_naive(r, g, b, x1, y1, x2, y2);
	}
	else {
		draw_steep_line_naive(r, g, b, x2, y2, x1, y1);
	}
}

void Engine::draw_shallow_line_naive(float r, float g, float b, int x1, int y1, int x2, int y2) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	float dydx = (float)(y2 - y1) / (x2 - x1);

	x1 = std::min(_frame.width - 1, std::max(0, x1));
	x2 = std::min(_frame.width - 1, std::max(0, x2));
	y1 = std::min(_frame.height - 1, std::max(0, y1));
	y2 = std::min(_frame.height - 1, std::max(0, y2));

	float y = y1;
	int screen_y, pixel;
	for (int x = x1; x < x2; ++x) {

		screen_y = (int)y;
		pixel = 4 * (_frame.width * screen_y + x);
		_frame.colorBufferData[pixel] = color[0];
		_frame.colorBufferData[pixel + 1] = color[1];
		_frame.colorBufferData[pixel + 2] = color[2];
		_frame.colorBufferData[pixel + 3] = color[3];

		y += dydx;
	}

}

void Engine::draw_steep_line_naive(float r, float g, float b, int x1, int y1, int x2, int y2) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	float dxdy = (float)(x2 - x1) / (y2 - y1);

	x1 = std::min(_frame.width - 1, std::max(0, x1));
	x2 = std::min(_frame.width - 1, std::max(0, x2));
	y1 = std::min(_frame.height - 1, std::max(0, y1));
	y2 = std::min(_frame.height - 1, std::max(0, y2));

	float x = x1;
	int screen_x, pixel;
	for (int y = y1; y < y2; ++y) {

		screen_x = (int)x;
		pixel = 4 * (_frame.width * y + screen_x);
		_frame.colorBufferData[pixel] = color[0];
		_frame.colorBufferData[pixel + 1] = color[1];
		_frame.colorBufferData[pixel + 2] = color[2];
		_frame.colorBufferData[pixel + 3] = color[3];

		x += dxdy;
	}

}

void Engine::draw_line_bresenham(float r, float g, float b, int x1, int y1, int x2, int y2) {

	if (x1 == x2) {
		if (y1 < y2) {
			draw_vertical_line(r, g, b, x1, y1, y2);
		}
		else {
			draw_vertical_line(r, g, b, x1, y2, y1);
		}
		return;
	}

	if (y1 == y2) {
		if (x1 < x2) {
			draw_horizontal_line_avx2(r, g, b, x1, x2, y1);
		}
		else {
			draw_horizontal_line_avx2(r, g, b, x2, x1, y1);
		}
		return;
	}

	if (abs(y2 - y1) < abs(x2 - x1)) {
		if (x1 < x2) {
			draw_shallow_line_bresenham(r, g, b, x1, y1, x2, y2);
		}
		else {
			draw_shallow_line_bresenham(r, g, b, x2, y2, x1, y1);
		}
		return;
	}

	if (y1 < y2) {
		draw_steep_line_bresenham(r, g, b, x1, y1, x2, y2);
	}
	else {
		draw_steep_line_bresenham(r, g, b, x2, y2, x1, y1);
	}
}

void Engine::draw_shallow_line_bresenham(float r, float g, float b, int x1, int y1, int x2, int y2) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	x1 = std::min(_frame.width - 1, std::max(0, x1));
	x2 = std::min(_frame.width - 1, std::max(0, x2));
	y1 = std::min(_frame.height - 1, std::max(0, y1));
	y2 = std::min(_frame.height - 1, std::max(0, y2));

	int dx = x2 - x1;
	int dy = y2 - y1;
	int yInc = 1;
	if (dy < 0) {
		yInc = -1;
		dy *= -1;
	}

	int D = 2 * dy - dx;
	int dDInc = 2 * (dy - dx);
	int dDNoInc = 2 * dy;

	int pixel;
	int y = y1;
	for (int x = x1; x < x2; ++x) {

		pixel = 4 * (_frame.width * y + x);
		_frame.colorBufferData[pixel] = color[0];
		_frame.colorBufferData[pixel + 1] = color[1];
		_frame.colorBufferData[pixel + 2] = color[2];
		_frame.colorBufferData[pixel + 3] = color[3];

		if (D > 0) {
			y += yInc;
			D += dDInc;
		}
		else {
			D += dDNoInc;
		}
	}

}

void Engine::draw_steep_line_bresenham(float r, float g, float b, int x1, int y1, int x2, int y2) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	unsigned char* color = convert_color(r, g, b);

	x1 = std::min(_frame.width - 1, std::max(0, x1));
	x2 = std::min(_frame.width - 1, std::max(0, x2));
	y1 = std::min(_frame.height - 1, std::max(0, y1));
	y2 = std::min(_frame.height - 1, std::max(0, y2));

	int dx = x2 - x1;
	int dy = y2 - y1;
	int xInc = 1;
	if (dx < 0) {
		xInc = -1;
		dx *= -1;
	}

	int D = 2 * dx - dy;
	int dDInc = 2 * (dx - dy);
	int dDNoInc = 2 * dx;

	int pixel;
	int x = x1;
	for (int y = y1; y < y2; ++y) {

		pixel = 4 * (_frame.width * y + x);
		_frame.colorBufferData[pixel] = color[0];
		_frame.colorBufferData[pixel + 1] = color[1];
		_frame.colorBufferData[pixel + 2] = color[2];
		_frame.colorBufferData[pixel + 3] = color[3];

		if (D > 0) {
			x += xInc;
			D += dDInc;
		}
		else {
			D += dDNoInc;
		}
	}

}

void Engine::draw_polygon_flat(float r, float g, float b, edgeTable polygon) {

	int x_start[480];
	int x_end[480];
	int y_min = 480;
	int y_max = 0;

	for (int i = 0; i < polygon.vertexCount; ++i) {

		vec4 vertex = polygon.vertices[i];

		if (vertex.data[1] < y_min) {
			y_min = std::max(0, (int)vertex.data[1]);
		}

		if (vertex.data[1] > y_max) {
			y_max = std::min(479, (int)vertex.data[1]);
		}
	}

	for (int y = y_min; y <= y_max; ++y) {
		x_start[y] = 640;
		x_end[y] = 0;
	}

	for (int j = 0; j < polygon.vertexCount; ++j) {
		int x1 = (int)polygon.vertices[j].data[0];
		int y1 = (int)polygon.vertices[j].data[1];
		int x2 = (int)polygon.vertices[(j + 1) % polygon.vertexCount].data[0];
		int y2 = (int)polygon.vertices[(j + 1) % polygon.vertexCount].data[1];

		if (abs(x2 - x1) < abs(y2 - y1)) {
			if (y1 < y2) {
				trace_steep_edge(x1, y1, x2, y2, x_start, x_end);
			}
			else {
				trace_steep_edge(x2, y2, x1, y1, x_start, x_end);
			}
		}
		else {
			if (x1 < x2) {
				trace_shallow_edge(x1, y1, x2, y2, x_start, x_end);
			}
			else {
				trace_shallow_edge(x2, y2, x1, y1, x_start, x_end);
			}
		}
	}

	for (int y = y_min; y <= y_max; ++y) {
		draw_horizontal_line_avx2(r, g, b, x_start[y], x_end[y], y);
	}
}

void Engine:: trace_shallow_edge(int x1, int y1, int x2, int y2, int* x_start, int* x_end) {

	int dx = x2 - x1;
	int dy = y2 - y1;
	int yInc = 1;
	if (dy < 0) {
		yInc = -1;
		dy *= -1;
	}

	int D = 2 * dy - dx;
	int dDInc = 2 * (dy - dx);
	int dDNoInc = 2 * dy;

	int pixel;
	int y = y1;
	for (int x = x1; x <= x2; ++x) {

		if (x < x_start[y] && y > 0 && y < 479) {
			x_start[y] = x;
		}

		if (x > x_end[y] && y > 0 && y < 479) {
			x_end[y] = x;
		}

		if (D > 0) {
			y += yInc;
			D += dDInc;
		}
		else {
			D += dDNoInc;
		}
	}
}

void Engine::trace_steep_edge(int x1, int y1, int x2, int y2, int* x_start, int* x_end) {

	int dx = x2 - x1;
	int dy = y2 - y1;
	int xInc = 1;
	if (dx < 0) {
		xInc = -1;
		dx *= -1;
	}

	int D = 2 * dx - dy;
	int dDInc = 2 * (dx - dy);
	int dDNoInc = 2 * dx;

	int pixel;
	int x = x1;
	for (int y = y1; y < y2; ++y) {

		if (x < x_start[y] && y > 0 && y < 479) {
			x_start[y] = x;
		}

		if (x > x_end[y] && y > 0 && y < 479) {
			x_end[y] = x;
		}

		if (D > 0) {
			x += xInc;
			D += dDInc;
		}
		else {
			D += dDNoInc;
		}
	}
}

void Engine::draw_polygon_blended(edgeTable polygon) {

	vertex* vertex_start = (vertex*)malloc(480 * sizeof(vertex));
	vertex* vertex_end = (vertex*)malloc(480 * sizeof(vertex));
	int y_min = 480;
	int y_max = 0;

	for (int i = 0; i < polygon.vertexCount; ++i) {

		vec4 vertex = polygon.vertices[i];

		if (vertex.data[1] < y_min) {
			y_min = std::max(0, (int)vertex.data[1]);
		}

		if (vertex.data[1] > y_max) {
			y_max = std::min(479, (int)vertex.data[1]);
		}
	}

	for (int y = y_min; y <= y_max; ++y) {
		vertex_start[y].x = 640;
		vertex_end[y].x = 0;
	}

	for (int j = 0; j < polygon.vertexCount; ++j) {

		vertex v1;
		v1.x = (int)polygon.vertices[j].data[0];
		v1.y = (int)polygon.vertices[j].data[1];
		v1.attributes = polygon.payloads[j];

		vertex v2;
		int k = (j + 1) % polygon.vertexCount;
		v2.x = (int)polygon.vertices[k].data[0];
		v2.y = (int)polygon.vertices[k].data[1];
		v2.attributes = polygon.payloads[k];

		if (abs(v2.x - v1.x) < abs(v2.y - v1.y)) {
			if (v1.y < v2.y) {
				interpolate_steep_edge(v1, v2, vertex_start, vertex_end);
			}
			else {
				interpolate_steep_edge(v2, v1, vertex_start, vertex_end);
			}
		}
		else {
			if (v1.x < v2.x) {
				interpolate_shallow_edge(v1, v2, vertex_start, vertex_end);
			}
			else {
				interpolate_shallow_edge(v2, v1, vertex_start, vertex_end);
			}
		}
	}

	for (int y = y_min; y <= y_max; ++y) {
		draw_horizontal_line_blended(vertex_start[y], vertex_end[y], y);
	}

	free(vertex_start);
	free(vertex_end);
}

void Engine::interpolate_shallow_edge(vertex v1, vertex v2, vertex* vertex_start, vertex* vertex_end) {

	int dx = v2.x - v1.x;
	int dy = v2.y - v1.y;
	int yInc = 1;
	if (dy < 0) {
		yInc = -1;
		dy *= -1;
	}

	int D = 2 * dy - dx;
	int dDInc = 2 * (dy - dx);
	int dDNoInc = 2 * dy;

	__m256 dPdx = _mm256_div_ps(
		_mm256_sub_ps(v2.attributes.lump, v1.attributes.lump),
		_mm256_set1_ps(dx)
	);
	vertex frag = v1;
	int y = v1.y;
	for (int x = v1.x; x <= v2.x; ++x) {

		if (x < vertex_start[y].x && y > 0 && y < 479) {
			vertex_start[y].x = x;
			vertex_start[y].attributes = frag.attributes;
		}

		if (x > vertex_end[y].x && y > 0 && y < 479) {
			vertex_end[y].x = x;
			vertex_end[y].attributes = frag.attributes;
		}

		if (D > 0) {
			y += yInc;
			D += dDInc;
		}
		else {
			D += dDNoInc;
		}

		frag.attributes.lump = _mm256_add_ps(frag.attributes.lump, dPdx);
	}

}

void Engine::interpolate_steep_edge(vertex v1, vertex v2, vertex* vertex_start, vertex* vertex_end) {

	int dx = v2.x - v1.x;
	int dy = v2.y - v1.y;
	int xInc = 1;
	if (dx < 0) {
		xInc = -1;
		dx *= -1;
	}

	int D = 2 * dx - dy;
	int dDInc = 2 * (dx - dy);
	int dDNoInc = 2 * dx;

	__m256 dPdy = _mm256_div_ps(
		_mm256_sub_ps(v2.attributes.lump, v1.attributes.lump),
		_mm256_set1_ps(dy)
	);
	vertex frag = v1;
	int x = v1.x;
	for (int y = v1.y; y <= v2.y; ++y) {

		if (x < vertex_start[y].x && y > 0 && y < 479) {
			vertex_start[y].x = x;
			vertex_start[y].attributes = frag.attributes;
		}

		if (x > vertex_end[y].x && y > 0 && y < 479) {
			vertex_end[y].x = x;
			vertex_end[y].attributes = frag.attributes;
		}

		if (D > 0) {
			x += xInc;
			D += dDInc;
		}
		else {
			D += dDNoInc;
		}

		frag.attributes.lump = _mm256_add_ps(frag.attributes.lump, dPdy);
	}
}

void Engine::draw_horizontal_line_blended(vertex v1, vertex v2, int y) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	int x1 = std::min(_frame.width - 1, std::max(0, v1.x));
	int x2 = std::min(_frame.width - 1, std::max(0, v2.x));
	y = std::min(_frame.height - 1, std::max(0, y));
	payload fragment = v1.attributes;
	__m256 dPdx = _mm256_div_ps(
		_mm256_sub_ps(v2.attributes.lump, v1.attributes.lump),
		_mm256_set1_ps(x2 - x1)
	);

	for (int x = x1; x < x2; ++x) {

		unsigned char* color = convert_color(fragment.data[0], fragment.data[1], fragment.data[2]);
		int pixel = 4 * (_frame.width * y + x);
		_frame.colorBufferData[pixel] = color[0];
		_frame.colorBufferData[pixel + 1] = color[1];
		_frame.colorBufferData[pixel + 2] = color[2];
		_frame.colorBufferData[pixel + 3] = color[3];

		fragment.lump = _mm256_add_ps(fragment.lump, dPdx);
	}
}

texture Engine::convert_texture(stbi_uc* textureData, int width, int height) {

	texture tex;
	tex.width = width;
	tex.height = height;
	tex.r = (float*)malloc(width * height * sizeof(float));
	tex.g = (float*)malloc(width * height * sizeof(float));
	tex.b = (float*)malloc(width * height * sizeof(float));

	for (int y = 0; y < height; ++y) {
		for (int x = 0; x < width; ++x) {
			tex.r[width * y + x] = (float)textureData[4 * (width * y + x)] / 255;
			tex.g[width * y + x] = (float)textureData[4 * (width * y + x) + 1] / 255;
			tex.b[width * y + x] = (float)textureData[4 * (width * y + x) + 2] / 255;
		}
	}

	return tex;
}

void Engine::draw_polygon_textured(edgeTable& polygon, texture& tex) {

	vertex* vertex_start = (vertex*)malloc(480 * sizeof(vertex));
	vertex* vertex_end = (vertex*)malloc(480 * sizeof(vertex));
	int y_min = 480;
	int y_max = 0;

	for (int i = 0; i < polygon.vertexCount; ++i) {

		vec4 vertex = polygon.vertices[i];

		if (vertex.data[1] < y_min) {
			y_min = std::max(0, (int)vertex.data[1]);
		}

		if (vertex.data[1] > y_max) {
			y_max = std::min(479, (int)vertex.data[1]);
		}
	}

	for (int y = y_min; y <= y_max; ++y) {
		vertex_start[y].x = 640;
		vertex_end[y].x = 0;
	}

	for (int j = 0; j < polygon.vertexCount; ++j) {

		vertex v1;
		v1.x = (int)polygon.vertices[j].data[0];
		v1.y = (int)polygon.vertices[j].data[1];
		v1.attributes = polygon.payloads[j];

		vertex v2;
		int k = (j + 1) % polygon.vertexCount;
		v2.x = (int)polygon.vertices[k].data[0];
		v2.y = (int)polygon.vertices[k].data[1];
		v2.attributes = polygon.payloads[k];

		if (abs(v2.x - v1.x) < abs(v2.y - v1.y)) {
			if (v1.y < v2.y) {
				interpolate_steep_edge(v1, v2, vertex_start, vertex_end);
			}
			else {
				interpolate_steep_edge(v2, v1, vertex_start, vertex_end);
			}
		}
		else {
			if (v1.x < v2.x) {
				interpolate_shallow_edge(v1, v2, vertex_start, vertex_end);
			}
			else {
				interpolate_shallow_edge(v2, v1, vertex_start, vertex_end);
			}
		}
	}

	for (int y = y_min; y <= y_max; ++y) {
		draw_horizontal_line_textured(vertex_start[y], vertex_end[y], y, tex);
	}

	free(vertex_start);
	free(vertex_end);
}

void Engine::draw_horizontal_line_textured(vertex v1, vertex v2, int y, texture& tex) {

	vkUtil::SwapChainFrame& _frame = swapchainFrames[frameNumber];

	int x1 = std::min(_frame.width - 1, std::max(0, v1.x));
	int x2 = std::min(_frame.width - 1, std::max(0, v2.x));
	y = std::min(_frame.height - 1, std::max(0, y));
	payload fragment = v1.attributes;
	__m256 dPdx = _mm256_div_ps(
		_mm256_sub_ps(v2.attributes.lump, v1.attributes.lump),
		_mm256_set1_ps(x2 - x1)
	);

	for (int x = x1; x < x2; ++x) {

		int u_left = std::min(tex.width - 1, std::max(0, (int)(tex.width * fragment.data[3])));
		int u_right = std::min(tex.width - 1, std::max(0, u_left + 1));
		float frac_u = tex.width * fragment.data[3] - u_left;
		float left = 1.0f - frac_u;
		float right = frac_u;

		int v_top = std::min(tex.height - 1, std::max(0, (int)(tex.height * fragment.data[4])));
		int v_bottom = std::min(tex.height - 1, std::max(0, v_top + 1));
		float frac_v = tex.height * fragment.data[4] - v_top;
		float top = 1.0f - frac_v;
		float bottom = frac_v;

		float r = fragment.data[0] * (
			top * (
				left * tex.r[v_top * tex.width + u_left] + right * tex.r[v_top * tex.width + u_right]
				)
			+ bottom * (
				left * tex.r[v_bottom * tex.width + u_left] + right * tex.r[v_bottom * tex.width + u_right]
				)
		);

		float g = fragment.data[1] * (
			top * (
				left * tex.g[v_top * tex.width + u_left] + right * tex.g[v_top * tex.width + u_right]
				)
			+ bottom * (
				left * tex.g[v_bottom * tex.width + u_left] + right * tex.g[v_bottom * tex.width + u_right]
				)
			);

		float b = fragment.data[2] * (
			top * (
				left * tex.b[v_top * tex.width + u_left] + right * tex.b[v_top * tex.width + u_right]
				)
			+ bottom * (
				left * tex.b[v_bottom * tex.width + u_left] + right * tex.b[v_bottom * tex.width + u_right]
				)
			);

		unsigned char* color = convert_color(r, g, b);
		int pixel = 4 * (_frame.width * y + x);
		_frame.colorBufferData[pixel] = color[0];
		_frame.colorBufferData[pixel + 1] = color[1];
		_frame.colorBufferData[pixel + 2] = color[2];
		_frame.colorBufferData[pixel + 3] = color[3];

		fragment.lump = _mm256_add_ps(fragment.lump, dPdx);
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