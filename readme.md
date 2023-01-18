# Vulkan Graphics Sandbox
## A minimal project for Hardware Accelerated Software Rendering

### Introduction
Back in the early days of game development, neat graphics APIs didn't exist. The only way to get a pixel on the screen was to write a number in an array at a certain RAM address. While modern APIs have certainly changed things for the better, the simple joy of being able to set random pixels whereever we want has become prohibitively expensive. The aim of this project is to allow per-pixel operations and classic graphics operations, while also taking advantage of OpenGL's capabilities.

### How does this differ from the OpenGL project?
I recently uploaded a project, pretty similar to this, implemented in OpenGL.
This Vulkan implementation is more lightweight, for instance the color buffer is
presented to the screen without a graphics pipeline, renderpass etc. It's simply
copied directly into the swapchain. We can also query the color format of the
swapchain in order to choose an appropriate color conversion function.

### Getting Started
For ease of use, the visual studio project files have been included. Visual Studio 2022 users need simply to open the project file and it's good to go. Currently the drawing functions are implemented in the Engine class.

### Adding/Modifying
The secondary purpose of this project is to encourage beginning programmers to get comfortable reading documentation and contributing. For this reason, the Engine class doesn't currently have a lot of functionality. Users are more than welcome to experiment with adding and optimising functions
