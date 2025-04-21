#include "imgui_vector_math.h"
#include <math.h>

ImVec2 ig_vec2_add(ImVec2 a, ImVec2 b)
{
	return (ImVec2 ){ a.x + b.x, a.y + b.y };
}

ImVec2 ig_vec2_diff(ImVec2 a, ImVec2 b)
{
	return (ImVec2){ a.x - b.x, a.y - b.y };
}

ImVec2 ig_vec2_scale(ImVec2 a, float scalar)
{
	return (ImVec2){ a.x * scalar, a.y * scalar };
}

float ig_vec2_length(ImVec2 a)
{
	return sqrtf(a.x * a.x + a.y * a.y);
}

ImVec2 ig_vec2_norm(ImVec2 a)
{
	float l = ig_vec2_length(a);
	return (ImVec2){ a.x / l, a.y / l };
}
