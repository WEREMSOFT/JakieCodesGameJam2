#include <stdlib.h>
#include "imguiHelper.h"
#include <GLFW/glfw3.h>

ImGuiIO *init_dear_imgui(GLFWwindow *window)
{
	 ImGuiIO *ioptr;

	// setup imgui
	igCreateContext(NULL);

	// set docking
	ioptr = igGetIO();
	ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
	ioptr->ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

#ifdef IMGUI_HAS_DOCK
	ioptr->ConfigFlags |= ImGuiConfigFlags_DockingEnable;	// Enable Docking
	ioptr->ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Enable Multi-Viewport / Platform Windows
#endif

// Shader version
#ifndef __EMSCRIPTEN__
	const char *glsl_version = "#version 130";
#else
	const char *glsl_version = "#version 300 es";
#endif

	ImGui_ImplGlfw_InitForOpenGL(window, true);
	ImGui_ImplOpenGL3_Init(glsl_version);
	igStyleColorsDark(NULL);

	return ioptr;
}

void begin_frame()
{
	glClearColor(0, 0, 0, 1.0);
	glClear(GL_COLOR_BUFFER_BIT);

	// render
	// start imgui frame
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	igNewFrame();
}

void end_frame(GLFWwindow *window)
{
	glfwPollEvents();
	glfwSwapBuffers(window);
}