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

	double renderTimeA = 0.0, renderTimeB = 0.0;
	int trialCount = 0;
	bool logged = false;
	float theta = 0.0f;

public:
	App(int width, int height, bool debug);
	~App();
	void run();

	void lines_test();
	void projection_test();
	void backface_test();
	void clipping_test();
	void flat_shading_test();
};