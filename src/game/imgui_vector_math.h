#ifndef __IMGUI_VECTOR_MATH_H__
#define _IMGUI_VECTOR_MATH_H__
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>

ImVec2 ig_vec2_add(ImVec2, ImVec2);
ImVec2 ig_vec2_diff(ImVec2, ImVec2);
ImVec2 ig_vec2_scale(ImVec2, float);
float ig_vec2_length(ImVec2);
ImVec2 ig_vec2_norm(ImVec2);

#endif