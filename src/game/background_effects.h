#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>
#include <math.h>
#include <stdlib.h>
#include "verlet.h"
#include "colors.h"
#include "imgui_vector_math.h"

#ifndef PI
#define PI 3.1415926535897932384626433832795f
#endif

extern ImDrawList *draw_list;

extern ImVec2 screen_size;
extern float star_speed;
extern int num_particles;
extern int num_links;
extern int num_near_constraints;
extern Particle particles[];
extern LinkConstraint links[];
extern UnidirectionalConstraint near_constraints[];
extern bool draw_plasma_ball_bool;
extern bool draw_starfield;

#define MAX_SEGMENTS 10
#define BRANCH_COUNT 3
#define MAX_BRANCHES 3 
#define CHAOS 1.3f 

void draw_lightning_branch(float cx, float cy, float angle, float length, int segments, int depth) {
    if (depth > MAX_BRANCHES) return; 

    float segment_length = length / segments;
    float x1 = cx;
    float y1 = cy;

    for (int i = 1; i <= segments; ++i) {
        float progress = (float)i / segments;
        float deviation = ((float)rand() / RAND_MAX - 0.5f) * CHAOS;

        float theta = angle + deviation;

        float x2 = cx + cosf(theta) * (segment_length * i);
        float y2 = cy + sinf(theta) * (segment_length * i);

		ImDrawList_AddLine(draw_list, (ImVec2){x1, y1},(ImVec2){x2, y2}, color_white, 3.);

        x1 = x2;
        y1 = y2;

        if (rand() % 100 < 50) {
            float branch_angle = theta + ((rand() % 40) - 20) * (PI / 180.0f);
            draw_lightning_branch(x2, y2, branch_angle, segment_length * 0.5f, segments / 2, depth + 1);
        }
    }
}

void draw_lightning(ImVec2 center, float radius) {
	static float base_angle = 0;
	base_angle += 0.01;
    for (int i = 0; i < BRANCH_COUNT; ++i) {
        float angle = base_angle + (2 * PI / BRANCH_COUNT) * i;
        draw_lightning_branch(center.x, center.y, angle, radius, MAX_SEGMENTS, 0);
    }
}

void draw_plasma_ball(float delta_time, float base_radious)
{
	if(draw_plasma_ball_bool)
	{
		static float phases[5] = {0.1, 0.2, 0.3, 0.4, 0.5};
		static float radiouses[5];
		static int color_index = 0;
		ImU32 colors[5] = {
			// create_rgb_color(0, 0, 0, 255),
			get_some_color_index(&color_index), 
			create_rgb_color(0, 23, 50, 255), 
			create_rgb_color(0, 23, 100, 255), 
			create_rgb_color(0, 23, 150, 255),
			create_rgb_color(0, 23, 250, 255)};
	
		for(int i = 4; i >= 0; i--)
		{
			phases[i] += 2.1 * delta_time;
			radiouses[i] = base_radious * .25 * (i + 1)+ sinf(phases[i]) * 50;
			ImDrawList_AddCircleFilled(draw_list, ig_vec2_scale(screen_size, 0.5), radiouses[i], colors[i], 30);
		}
	
		draw_lightning(ig_vec2_scale(screen_size, 0.5), radiouses[4]);
	}
	star_speed = base_radious * 0.005;
	
	if(draw_starfield)
		update_and_draw_starfield(screen_size, delta_time);
}

void draw_background_effects(float delta_time)
{
	EnergyOutput eo = calculate_kinetic_energy(particles, links, num_particles, num_links, delta_time);
	update_energy_display(particles, links, num_particles, num_links, delta_time);
	draw_plasma_ball(delta_time, eo.linked_particles);
}