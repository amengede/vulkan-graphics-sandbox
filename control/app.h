#pragma once
#include "../config.h"
#include "../view/engine.h"

class App {

private:
	Engine* graphicsEngine;
	GLFWwindow* window;

	double lastTime, currentTime;
	int numFrames;
	float frameTime;

	void build_glfw_window(int width, int height);

	void calculateFrameRate();

public:
	App(int width, int height, bool debug);
	~App();
	void run();
};