#ifndef __COLORS_H__
#define	__COLORS_H__
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>

static ImU32 create_rgb_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

ImU32 color_red;
ImU32 color_red_alpha;
ImU32 color_white;
ImU32 color_gray;
ImU32 color_gray_alpha;
ImU32 color_green;
ImU32 color_blue;
ImU32 color_yellow;
ImU32 color_purple;

static void init_colors()
{
	color_red = create_rgb_color(255, 0, 0, 255);
	color_red_alpha = create_rgb_color(255, 0, 0, 128);
	color_white = create_rgb_color(255, 255, 255, 255);
	color_green = create_rgb_color(0, 255, 0, 255);
	color_purple = create_rgb_color(255, 0, 255, 255);
	color_yellow = create_rgb_color(255, 255, 0, 255);
	color_blue = create_rgb_color(0, 0, 255, 255);
	color_gray = create_rgb_color(200, 200, 200, 255);
	color_gray_alpha = create_rgb_color(200, 200, 200, 128);
}

ImU32 get_some_color_index(int *color_index)
{
	int color = (*color_index)++ % 6;
	ImU32 colors[] = { color_red, color_white, color_gray, color_green, color_blue, color_yellow, color_purple };
	return colors[color];
}

ImU32 get_some_color()
{
	ImU32 colors[] = { color_red, color_white, color_gray, color_green, color_blue, color_yellow, color_purple };
	static int color_index = 0;
	return colors[color_index++ % 6];
}

#endif