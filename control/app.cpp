#include "app.h"
#include "logging.h"
#include <chrono>
#include <math.h>
#include "../linear_algebros.h"

/**
* Construct a new App.
* 
* @param width	the width of the window
* @param height the height of the window
* @param debug	whether to run the app with vulkan validation layers and extra print statements
*/
App::App(int width, int height, bool debug) {

	vkLogging::Logger::get_logger()->set_debug_mode(debug);

	build_glfw_window(width, height);

	graphicsEngine = new Engine(width, height, window);

}

/**
* Build the App's window (using glfw)
* 
* @param width		the width of the window
* @param height		the height of the window
* @param debugMode	whether to make extra print statements
*/
void App::build_glfw_window(int width, int height) {

	std::stringstream message;

	//initialize glfw
	glfwInit();

	//no default rendering client, we'll hook vulkan up
	//to the window later
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	//resizing breaks the swapchain, we'll disable it for now
	glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

	//GLFWwindow* glfwCreateWindow (int width, int height, const char *title, GLFWmonitor *monitor, GLFWwindow *share)
	if (window = glfwCreateWindow(width, height, "ID Tech 12", nullptr, nullptr)) {
		message << "Successfully made a glfw window called \"ID Tech 12\", width: " << width << ", height: " << height;
		vkLogging::Logger::get_logger()->print(message.str());
	}
	else {
		vkLogging::Logger::get_logger()->print("GLFW window creation failed");
	}
}

/**
* Start the App's main loop
*/
void App::run() {

	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		//graphicsEngine->clear_screen(0.0, 0.0, 0.0);
		graphicsEngine->clear_screen_avx2(0.0, 0.0, 0.0);
		//lines_test();
		projection_test();
		graphicsEngine->render();

		calculateFrameRate();
	}
}

void App::lines_test() {

	if (trialCount < 1000) {
		auto start = std::chrono::steady_clock::now();
		//shallow, +ve slope
		graphicsEngine->draw_line_naive(0.0f, 1.0f, 1.0f, 20, 32, 420, 128);
		//steep, +ve slope
		graphicsEngine->draw_line_naive(0.0f, 1.0f, 1.0f, 20, 32, 420, 628);
		//shallow, -ve slope
		graphicsEngine->draw_line_naive(0.0f, 1.0f, 1.0f, 420, 32, 20, 128);
		//steep, -ve slope
		graphicsEngine->draw_line_naive(0.0f, 1.0f, 1.0f, 420, 32, 20, 628);
		auto end = std::chrono::steady_clock::now();
		renderTimeA += 0.001 * std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

		start = std::chrono::steady_clock::now();
		//shallow, +ve slope
		graphicsEngine->draw_line_bresenham(1.0f, 0.0f, 1.0f, 220, 32, 620, 128);
		//steep, +ve slope
		graphicsEngine->draw_line_bresenham(1.0f, 0.0f, 1.0f, 220, 32, 620, 628);
		//shallow, -ve slope
		graphicsEngine->draw_line_bresenham(1.0f, 0.0f, 1.0f, 620, 32, 220, 128);
		//steep, -ve slope
		graphicsEngine->draw_line_bresenham(1.0f, 0.0f, 1.0f, 620, 32, 220, 628);
		end = std::chrono::steady_clock::now();
		renderTimeB += 0.001 * std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();
		trialCount += 1;
	}
	else {
		//shallow, +ve slope
		graphicsEngine->draw_line_naive(0.0f, 1.0f, 1.0f, 20, 32, 420, 128);
		//steep, +ve slope
		graphicsEngine->draw_line_naive(0.0f, 1.0f, 1.0f, 20, 32, 420, 628);
		//shallow, -ve slope
		graphicsEngine->draw_line_naive(0.0f, 1.0f, 1.0f, 420, 32, 20, 128);
		//steep, -ve slope
		graphicsEngine->draw_line_naive(0.0f, 1.0f, 1.0f, 420, 32, 20, 628);

		//shallow, +ve slope
		graphicsEngine->draw_line_bresenham(1.0f, 0.0f, 1.0f, 220, 32, 620, 128);
		//steep, +ve slope
		graphicsEngine->draw_line_bresenham(1.0f, 0.0f, 1.0f, 220, 32, 620, 628);
		//shallow, -ve slope
		graphicsEngine->draw_line_bresenham(1.0f, 0.0f, 1.0f, 620, 32, 220, 128);
		//steep, -ve slope
		graphicsEngine->draw_line_bresenham(1.0f, 0.0f, 1.0f, 620, 32, 220, 628);

		if (!logged) {
			std::cout << "Naive render took " << (int)renderTimeA << " us." << std::endl;
			std::cout << "Bresenham render took " << (int)renderTimeB << " us." << std::endl;
			logged = true;
		}
	}
}

void App::projection_test() {
	const int pointCount = 8;
	vec4 vertices[pointCount] = {
		{ 0.75f,  0.75f, -2.0f, 1.0f}, //0
		{-0.75f,  0.75f, -2.0f, 1.0f}, //1
		{-0.75f, -0.75f, -2.0f, 1.0f}, //2
		{ 0.75f, -0.75f, -2.0f, 1.0f}, //3

		{-0.75f,  0.75f, -3.5f, 1.0f}, //4
		{ 0.75f,  0.75f, -3.5f, 1.0f}, //5
		{ 0.75f, -0.75f, -3.5f, 1.0f}, //6
		{-0.75f, -0.75f, -3.5f, 1.0f}, //7
	};
	vec4 transformedVertices[pointCount];

	const int edgeCount = 12;
	int edge_a[edgeCount] = {
		0, 1, 2, 3,
		4, 5, 6, 7,
		1, 5, 3, 7
	};

	int edge_b[edgeCount] = {
		1, 2, 3, 0,
		5, 6, 7, 4,
		4, 0, 6, 2
	};

	if (!logged) {
		std::cout << "----    Cube Vertices (Initial):    ----" << std::endl;
		for (int i = 0; i < pointCount; ++i) {
			std::cout << "("
				<< vertices[i].data[0] << ", "
				<< vertices[i].data[1] << ", "
				<< vertices[i].data[2] << ", "
				<< vertices[i].data[3] << ")" << std::endl;
		}
	}

	mat4 projection = linalgMakePerspectiveProjection(45.0f, (float)640 / 480, 0.1f, 10.0f);
	for (int i = 0; i < pointCount; ++i) {
		transformedVertices[i] = linalgMulMat4Vec4(projection, vertices[i]);
		transformedVertices[i].data[0] = transformedVertices[i].data[0] / transformedVertices[i].data[3];
		transformedVertices[i].data[1] = transformedVertices[i].data[1] / transformedVertices[i].data[3];
		transformedVertices[i].data[2] = transformedVertices[i].data[2] / transformedVertices[i].data[3];
	}

	if (!logged) {
		std::cout << "----    Cube Vertices (Projected):    ----" << std::endl;
		for (int i = 0; i < pointCount; ++i) {
			std::cout << "("
				<< transformedVertices[i].data[0] << ", "
				<< transformedVertices[i].data[1] << ", "
				<< transformedVertices[i].data[2] << ", "
				<< transformedVertices[i].data[3] << ")" << std::endl;
		}
	}

	for (int i = 0; i < pointCount; ++i) {
		transformedVertices[i].data[0] = 320 + 320 * transformedVertices[i].data[0];
		transformedVertices[i].data[1] = 240 - 240 * transformedVertices[i].data[1];
	}

	if (!logged) {
		std::cout << "----    Screen Coordinates:    ----" << std::endl;
		for (int i = 0; i < pointCount; ++i) {
			std::cout << "("
				<< (int)transformedVertices[i].data[0] << ", "
				<< (int)transformedVertices[i].data[1] << ")" << std::endl;
		}
	}

	for (int i = 0; i < edgeCount; ++i) {
		graphicsEngine->draw_line_bresenham(
			1.0f, 1.0f, 1.0f,
			transformedVertices[edge_a[i]].data[0], transformedVertices[edge_a[i]].data[1],
			transformedVertices[edge_b[i]].data[0], transformedVertices[edge_b[i]].data[1]
		);
	}

	logged = true;
}

/**
* Calculates the App's framerate and updates the window title
*/
void App::calculateFrameRate() {
	currentTime = glfwGetTime();
	double delta = currentTime - lastTime;

	if (delta >= 1) {
		int framerate{ std::max(1, int(numFrames / delta)) };
		std::stringstream title;
		title << "Running at " << framerate << " fps.";
		glfwSetWindowTitle(window, title.str().c_str());
		lastTime = currentTime;
		numFrames = -1;
		frameTime = float(1000.0 / framerate);
	}

	++numFrames;
}

/**
* App destructor.
*/
App::~App() {
	delete graphicsEngine;
}