#ifndef __COLORS_H__
#define	__COLORS_H__
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>

static ImU32 create_rgb_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

ImU32 color_rojo;
ImU32 color_blanco;
ImU32 color_gris;
ImU32 color_verde;
ImU32 color_azul;
ImU32 color_amarillo;
ImU32 color_purpura;

static void init_colors()
{
	color_rojo = create_rgb_color(255, 0, 0, 255);
	color_blanco = create_rgb_color(255, 255, 255, 255);
	color_verde = create_rgb_color(0, 255, 0, 255);
	color_purpura = create_rgb_color(255, 0, 255, 255);
	color_amarillo = create_rgb_color(255, 255, 0, 255);
	color_azul = create_rgb_color(0, 0, 255, 255);
	color_gris = create_rgb_color(200, 200, 200, 255);
}

#endif