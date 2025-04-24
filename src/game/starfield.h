#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <stdlib.h>
#include <cimgui_impl.h>
#include "colors.h"

#define MAX_STARS 1024
extern ImDrawList *draw_list;

typedef struct {
    float x, y; // posición en espacio -1 a 1
    float z;    // profundidad (0.0 = cerca, 1.0 = lejos)
} Star;

static Star stars[MAX_STARS];
static int num_stars = 500;
static float star_speed = 0.5f;

void reset_star(Star* s) {
    s->x = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    s->y = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
    s->z = 1.0f;
}

void init_stars(ImDrawList *drw_list) {
	draw_list = drw_list;
    for (int i = 0; i < MAX_STARS; ++i)
        reset_star(&stars[i]);
}

void update_and_draw_starfield(ImVec2 screen_size, float delta_time) {
    ImDrawList* draw_list = igGetBackgroundDrawList_Nil();
    ImVec2 center = { screen_size.x * 0.5f, screen_size.y * 0.5f };

    for (int i = 0; i < num_stars && i < MAX_STARS; ++i) {
        Star* s = &stars[i];

        s->z -= delta_time * star_speed;
        if (s->z <= 0.01f) reset_star(s);

        float sx = (s->x / s->z) * center.x + center.x;
        float sy = (s->y / s->z) * center.y + center.y;

        if (sx < 0 || sx >= screen_size.x || sy < 0 || sy >= screen_size.y) {
            reset_star(s);
            continue;
        }

        if (star_speed > 0.3f) {
            // Dibuja una línea si la velocidad es alta
            float prev_z = s->z + delta_time * star_speed;
            float px = (s->x / prev_z) * center.x + center.x;
            float py = (s->y / prev_z) * center.y + center.y;
			ImDrawList_AddLine(draw_list, (ImVec2){px, py}, (ImVec2){sx, sy}, color_blanco, 2);
        } else {
            ImDrawList_AddCircleFilled(draw_list, (ImVec2){sx, sy}, 1.5f, color_blanco, 6);
        }
    }
}

void starfield_ui() {
    if (igBegin("Starfield Settings", NULL, 0)) {
        igSliderInt("Number of Stars", &num_stars, 10, MAX_STARS, "%d", 0);
        igSliderFloat("Speed", &star_speed, 0.01f, 2.0f, "%.2f", 0);
        igEnd();
    }
}