#ifndef __IMGUI_HELPER_H__
#define __IMGUI_HELPER_H__
#include <GLFW/glfw3.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>
#ifndef igGetIO
#define igGetIO igGetIO_Nil
#endif

ImGuiIO *init_dear_imgui(GLFWwindow *window);
void begin_frame();
void end_frame(GLFWwindow *window);

#endif