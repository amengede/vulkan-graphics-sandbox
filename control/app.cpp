#include "app.h"
#include "logging.h"

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

		//graphicsEngine->clear_screen(1.0, 1.0, 0.0);
		graphicsEngine->clear_screen_avx2(1.0, 0.0, 0.0);
		graphicsEngine->render();

		calculateFrameRate();
	}
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