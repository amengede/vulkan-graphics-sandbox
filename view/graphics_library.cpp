#include "graphics_library.h"

unsigned char* convert_to_r8g8b8a8_unorm(float r, float g, float b) {

	unsigned char color[4];

	r = std::max(std::min(r, 0.99f), 0.0f);
	g = std::max(std::min(g, 0.99f), 0.0f);
	b = std::max(std::min(b, 0.99f), 0.0f);

	color[0] = static_cast<unsigned char>(r * 0xFF);
	color[1] = static_cast<unsigned char>(g * 0xFF);
	color[2] = static_cast<unsigned char>(b * 0xFF);
	color[3] = static_cast<unsigned char>(0xFF);

	return color;
}

unsigned char* convert_to_b8g8r8a8_unorm(float r, float g, float b) {

	unsigned char color[4];

	r = std::max(std::min(r, 0.99f), 0.0f);
	g = std::max(std::min(g, 0.99f), 0.0f);
	b = std::max(std::min(b, 0.99f), 0.0f);

	color[0] = static_cast<unsigned char>(b * 0xFF);
	color[1] = static_cast<unsigned char>(g * 0xFF);
	color[2] = static_cast<unsigned char>(r * 0xFF);
	color[3] = static_cast<unsigned char>(0xFF);

	return color;
}