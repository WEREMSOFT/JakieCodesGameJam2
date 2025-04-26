#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/param.h>
#include <pthread.h>
#include <stdbool.h>
#include <soloud_c.h>
#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>
#ifndef igGetIO
#define igGetIO igGetIO_Nil
#endif

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <stdint.h>

#ifndef igGetIO
#define igGetIO igGetIO_Nil
#endif

#include "game.h"
#include "image.h"
#include "glfwHelper.h"
#include "imguiHelper.h"
#include "verlet.h"
#include "imgui_vector_math.h"
#include "energy_graph.h"
#include "browser_interaction.h"
#include "starfield.h"
#include "background_effects.h"
#include "serialization.h"
#include "help_about_windows.h"
#include "sound.h"

#define PARTICLE_FACES 30
#define CONSTRAINT_FACES 64
#define SUB_CYCLES 1

Sound sound;

static GLFWwindow *window;
static ImGuiIO *ioptr;

int num_particles = 0;
int last_particle_added = -1;
int num_far_constraints = 0;
int num_near_constraints = 0;
int num_links = 0;
int num_free_particles;
int num_engines = 0;

bool fixed_generators = false;

#define MAX_PARTICLES 4000

#ifndef PI
#define PI 3.1415926535897932384626433832795f
#endif

LinkConstraint links[MAX_PARTICLES];
Particle particles[MAX_PARTICLES];
Engine engines[MAX_PARTICLES];
int free_particles_queue[MAX_PARTICLES] = {0};
UnidirectionalConstraint far_constraints[MAX_PARTICLES];
UnidirectionalConstraint near_constraints[MAX_PARTICLES];
GameStateEnum state = GAME_STATE_RUNNING;
ImDrawList *draw_list;
ImVec2 screen_size = {};
#define MAX_LEVEL_TEXT 1000000
static char levelTextBuffer[MAX_LEVEL_TEXT] = "";

static SpacePartitionCell space_partition_grid[50][50] = {0};

int cellSize = 20;

EnergyOutput energy_output = {0.000001, 0.000001};

bool space_partitioning = false;
bool draw_plasma_ball_bool = true;
bool draw_starfield = true;
bool inject_particles = true;
bool constrain_to_screen = false;
bool destroy_particels_outside_screen = true;
bool draw_part_grid = false;
bool show_energy_window = false;
bool show_emisor_properties_window = true;
bool show_how_to_play_window = true;
bool show_about_window = false;
bool developer_mode = false;
bool already_won = false;

float motor_torque = 250.;

ImVec2 particle_emissor_initial_velocity = { -3., 0 };
float particle_emissor_base_radious = 6.5;
ImVec2 emissor_position = {0};

static int add_particle(Particle particle, bool user_created)
{
	particle.enabled = true;
	particle.user_created = user_created;
	if(num_free_particles>0)
	{
		int free_particle = free_particles_queue[--num_free_particles];
		last_particle_added = free_particle;
		particles[free_particle] = particle;
		return free_particle;
	}
	last_particle_added = num_particles;
	particles[num_particles++] = particle;
	return num_particles-1;
}

static void delete_particle(int particle)
{
	particles[particle].enabled = false;
	free_particles_queue[num_free_particles++] = particle;
}

static int get_particle_under_the_mouse()
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);

	for(int i = 0; i < num_particles; i++)
	{
		if(!particles[i].enabled) continue;
		float dist = ig_vec2_length(ig_vec2_diff(particles[i].position, mouse_pos));
		if(dist < particles[i].radious) return i;
	}

	return -1;
}

void gear_apply_motor_force(Engine engine)
{
    Particle *center = &particles[engine.particle_id_center];

    for (int i = 0; i < engine.num_particles; i++)
    {
        Particle *p = &particles[engine.particles_border[i]];
        if (!p->enabled) continue;

        // Vector desde el centro a la partícula
        ImVec2 dir = { p->position.x - center->position.x, p->position.y - center->position.y };

        // Vector perpendicular (para girar)
        ImVec2 perp = { -dir.y, dir.x };

        // Normalizar
        float len = sqrtf(perp.x * perp.x + perp.y * perp.y);
        if (len == 0.0f) continue; // por seguridad
        perp.x /= len;
        perp.y /= len;

        // Aplicar aceleración
        p->acceleration.x += perp.x * engine.strength;
        p->acceleration.y += perp.y * engine.strength;
    }
}

static void build_partition_grid(void)
{
	int matrixWidth = screen_size.x / cellSize;
	int matrixHeight = screen_size.y / cellSize;

	for (int p = 0; p < num_particles; p++)
	{
		Particle particle = particles[p];

		if(!particle.enabled) continue;

		int i = (int)(particle.position.x / cellSize);
		int j = (int)(particle.position.y / cellSize);

		if (i >= 0 && i < matrixWidth && j >= 0 && j < matrixHeight)
		{
			SpacePartitionCell* cell = &space_partition_grid[i][j];
			cell->particles[cell->particleCount++] = p;
		}
	}
}

static void reset_partition_grid()
{
	memset(space_partition_grid, 0, sizeof(SpacePartitionCell) * 50 * 50);
}

static void import_game(void)
{
	#ifdef __EMSCRIPTEN__
	load_level_from_url(levelTextBuffer, MAX_LEVEL_TEXT);
	if(deserialize_level(levelTextBuffer) > -1)
	{
		already_won = true;
	}
	printf("deserialization complete\n");
	#endif
}

static void init(int width, int height, int window_scale, bool full_screen)
{
	screen_size.x = width;
	screen_size.y = height;
	window = glfwCreateAndInitWindow(width, height, window_scale, full_screen);
	glfwSwapInterval(1); 
	ioptr = init_dear_imgui(window);
	particles[0] = (Particle){(ImVec2){300, 20}, (ImVec2){300, 20}, (ImVec2){0, 0}, 1.};
	init_colors();
	ioptr->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	emissor_position = (ImVec2) {.x = screen_size.x/2, .y = screen_size.y/2-550};
	draw_list = igGetBackgroundDrawList_Nil();
	init_stars(draw_list);
	import_game();
}

static void export_game(void)
{
	#ifdef __EMSCRIPTEN__
	clear_output_console();
	serialize_level(levelTextBuffer, MAX_LEVEL_TEXT);
	save_level_to_url(levelTextBuffer);
	#endif
	
}

void draw_energy_efficiency_bar(float efficiency_ratio) 
{
	efficiency_ratio = fmax(0.0001, efficiency_ratio);
	igSetNextWindowPos((ImVec2) {0, 20}, ImGuiCond_Once, (ImVec2){0});

    if (igBegin("Efficiency", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {

		ImDrawList* draw_list = igGetWindowDrawList();
        ImVec2 pos = {0};
		igGetCursorScreenPos(&pos);
        float width = 50.0f;
        float height = 535.0f;

        ImU32 bg_color = create_rgb_color(80, 80, 80, 255);
        ImU32 fg_color = create_rgb_color(100, 255, 100, 255);
        ImU32 border_color = create_rgb_color(255, 255, 255, 255);
        ImU32 text_color = create_rgb_color(255, 255, 255, 255);

        ImVec2 p0 = pos;
        ImVec2 p1 = { pos.x + width, pos.y + height };
		ImDrawList_AddRectFilled(draw_list, p0, p1, bg_color, 4.0f, 0);
		ImDrawList_AddRectFilled(draw_list, p0, p1, border_color, 4.0f, 0);

        if (efficiency_ratio < 0.0f) efficiency_ratio = 0.0f;
        if (efficiency_ratio > 1.0f) efficiency_ratio = 1.0f;

        float fill_height = height * efficiency_ratio;
        ImVec2 fill_p0 = { pos.x, pos.y + (height - fill_height) };
        ImVec2 fill_p1 = { pos.x + width, pos.y + height };
       ImDrawList_AddRectFilled(draw_list, fill_p0, fill_p1, fg_color, 4.0f, 0);

        igDummy((ImVec2){width, height + 5});

    }
	igEnd();
}

static void draw_start_stop_controls()
{
	igSetNextWindowPos((ImVec2){265,546}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});

	igBegin("Controls", NULL, ImGuiWindowFlags_AlwaysAutoResize);


	if (igButton("Run", (ImVec2){0, 0}))
	{
		soundPlaySfx(sound, SFX_COIN);
		state = GAME_STATE_RUNNING;
	}
	igSameLine(0.0f, -1.0f);
	if (igButton("Stop", (ImVec2){0, 0}))
	{
		soundPlaySfx(sound, SFX_COIN);
		state = GAME_STATE_READY;
	}
	igSameLine(0.0f, -1.0f);
	if (igButton("Delete Particle", (ImVec2){0, 0}))
	{
		soundPlaySfx(sound, SFX_HURT);
		state = GAME_STATE_DELETE_PARTICLE;
	}
	igSameLine(0.0f, -1.0f);
	if (igButton("Particle", (ImVec2){0, 0}))
	{
		soundPlaySfx(sound, SFX_COIN);
		state = GAME_STATE_CREATING_STANDARD_PARTICLE;
	}
	igSameLine(0.0f, -1.0f);
	if (igButton("Link", (ImVec2){0, 0}))
	{
		soundPlaySfx(sound, SFX_COIN);
		state = GAME_STATE_CREATING_LINK_CONSTRAINT;
	}
	igSameLine(0.0f, -1.0f);
	if (igButton("Anchor", (ImVec2){0, 0}))
	{
		soundPlaySfx(sound, SFX_COIN);
		state = GAME_STATE_CREATING_FIXED_PARTICLE;
	}
	igSameLine(0.0f, -1.0f);
	if (igButton("Generator", (ImVec2){0, 0}))
	{
		soundPlaySfx(sound, SFX_COIN);
		state = GAME_STATE_CREATING_FIXED_SIZE_GEAR;
	}
	igSameLine(0.0f, -1.0f);
	if (igButton("Expulsor", (ImVec2){0, 0}))
	{
		soundPlaySfx(sound, SFX_COIN);
		state = GAME_STATE_CREATING_NEAR_CONSTRAINT;
	}
	igSameLine(0.0f, -1.0f);
	igCheckbox("Generators Are Static", &fixed_generators);

	igEnd();
}

static bool draw_gui()
{
	if (igBeginMainMenuBar())
	{
		if (igBeginMenu("Edit", true))
		{
			state = GAME_STATE_READY;
			if (igMenuItem_Bool("Run", "", false, true))
			{
				soundPlaySfx(sound, SFX_COIN);
				state = GAME_STATE_RUNNING;
			}
			if (igMenuItem_Bool("Reset", "", false, true))
			{
				soundPlaySfx(sound, SFX_HURT);

				already_won = false;
				for(int i = 0; i < num_particles; i++)
				{
					if(particles[i].user_created)
					{
						delete_particle(i);
					}
				}

				num_engines = num_near_constraints = num_links = num_far_constraints = 0;
			}
			igSeparator();
			if(igMenuItem_Bool("Delete Particle", "", NULL, true))
			{
				soundPlaySfx(sound, SFX_HURT);
				state = GAME_STATE_DELETE_PARTICLE;
			}
			igSeparator();
			if (igMenuItem_BoolPtr("Draw Plasma Ball", "", &draw_plasma_ball_bool, true));
			if (igMenuItem_BoolPtr("Draw StarField", "", &draw_starfield, true));
			igSeparator();
			if(developer_mode)
			{
				if (igMenuItem_BoolPtr("Constrain Particles To Screen", "", &constrain_to_screen, true));
				if (igMenuItem_BoolPtr("Destroy Particles Out Of Screen", "", &destroy_particels_outside_screen, true));
				if (igMenuItem_BoolPtr("Space Partitioning", "", &space_partitioning, true));
				if (igMenuItem_BoolPtr("Inject Particles", "", &inject_particles, true));
				if (igMenuItem_BoolPtr("Draw Partition Grid", "", &draw_part_grid, true));
				igSeparator();
				#ifdef __EMSCRIPTEN__
				if (igMenuItem_Bool("Write Broser Url", "", false, true))
				{
					set_browser_url("?level=3&mode=edit");
				}
	
				if (igMenuItem_Bool("Get Broser Url", "", false, true))
				{
					printf("%s\n", get_browser_url());
				}
	
				if (igMenuItem_Bool("Get Console Text", "", false, true))
				{
					clear_output_console();
					printf("\n##########\n%s\n", get_console_text(levelTextBuffer));
				}
	
				igSeparator();
				#endif
			}
			
			if (igMenuItem_Bool("Export Game", "", NULL, true))
			{
				export_game();
			}
			if (igMenuItem_Bool("Import Game", "", NULL, true))
			{
				import_game();
			}
			igSeparator();
			igMenuItem_BoolPtr("Developer Mode(only if you know what you are doing)", "", &developer_mode, true);
			igEndMenu();
		}

		if (igBeginMenu("Particles", true))
		{
			state = GAME_STATE_READY;
			if (igMenuItem_Bool("Add Particle(defines size)", "", false, true))
			{
				state = GAME_STATE_CREATING_PARTICLE;
			}
			if (igMenuItem_Bool("Add Fixed Particle", "", false, true))
			{
				state = GAME_STATE_CREATING_FIXED_PARTICLE;
			}
			if (igMenuItem_Bool("Add Particle with a standard size", "", false, true))
			{
				state = GAME_STATE_CREATING_STANDARD_PARTICLE;
			}
			igEndMenu();
		}

		if (igBeginMenu("Constraints/Links", true))
		{
			state = GAME_STATE_READY;
			if (igMenuItem_Bool("Add Link Between Particles", "", false, true))
			{
				state = GAME_STATE_CREATING_LINK_CONSTRAINT;
			}
			if(developer_mode){
				if (igMenuItem_Bool("Add Global Restriction Field", "", false, true))
				{
					state = GAME_STATE_CREATING_FAR_CONSTRAINT;
				}
			}
			if (igMenuItem_Bool("Add Expulsion Field", "", false, true))
			{
				state = GAME_STATE_CREATING_NEAR_CONSTRAINT;
			}
			igEndMenu();
		}

		if (igBeginMenu("Generators", true))
		{
			state = GAME_STATE_READY;
			if (igMenuItem_Bool("Add Fixed Size Generator", "", false, true))
			{
				state = GAME_STATE_CREATING_FIXED_SIZE_GEAR;
			}
			if (igMenuItem_Bool("Add Generator(defines size)", "", false, true))
			{
				state = GAME_STATE_CREATING_GEAR;
			}
			igEndMenu();
		}

		if(developer_mode)
		{
			if (igBeginMenu("Motors", true))
			{
				state = GAME_STATE_READY;
				if (igMenuItem_Bool("Add Fixed Size Motor", "", false, true))
				{
					state = GAME_STATE_CREATING_FIXED_SIZE_ENGINE;
				}
				if (igMenuItem_Bool("Add Motor(defines size)", "", false, true))
				{
					state = GAME_STATE_CREATING_ENGINE;
				}
				igEndMenu();
			}
		}
		

		if (igBeginMenu("Information", true))
		{
			state = GAME_STATE_READY;
			igMenuItem_BoolPtr("Show Energy Efficience", "", &show_energy_window, true);
			igMenuItem_BoolPtr("Show Particle Emitter Properties", "", &show_emisor_properties_window, true);
			igEndMenu();
		}

		if (igBeginMenu("Help", true))
		{
			state = GAME_STATE_READY;
			if (igMenuItem_BoolPtr("About", "", &show_about_window, true))
			{
			}
			if (igMenuItem_BoolPtr("How To Play", "", &show_how_to_play_window, true))
			{
			}
			igEndMenu();
		}

		static float average_fps = 0.0f;
		average_fps = (igGetIO()->Framerate + average_fps) / 2;

		char fps_text[64];
		snprintf(fps_text, sizeof(fps_text), "FPS: %.1f", average_fps);

		if (igBeginMenu(fps_text, true))
			igEndMenu();

		char particle_count_txt[64];
		snprintf(particle_count_txt, sizeof(particle_count_txt), "Particles: %d", num_particles - num_free_particles);

		if (igBeginMenu(particle_count_txt, true))
			igEndMenu();
		igEndMainMenuBar();

		if(show_energy_window  && !show_how_to_play_window)
		{
			igSetNextWindowPos((ImVec2) {0, 454}, ImGuiCond_FirstUseEver, (ImVec2){0});
			draw_energy_graph(&show_energy_window);
		}

		EnergyOutput eo = calculate_kinetic_energy(particles, links, num_particles, num_links, ioptr->DeltaTime);

		if(eo.linked_particles / eo.free_particles > .6 && !already_won && !developer_mode)
		{
			soundPlaySpeech(sound, SPEECH_YOU_WIN);
			already_won = true;
			state = GAME_STATE_WON;
		}

		if(!developer_mode)
			draw_energy_efficiency_bar(eo.linked_particles / eo.free_particles);

		if(show_emisor_properties_window && !show_how_to_play_window)
		{
			igSetNextWindowPos((ImVec2) {500,19}, ImGuiCond_Once, (ImVec2){0, 0});
			igSetNextWindowSize((ImVec2) {270,105}, ImGuiCond_Once);
			igBegin("Particle Emisor Properties", &show_emisor_properties_window, ImGuiWindowFlags_AlwaysAutoResize);
			igText("Speed X:");
			igSliderFloat("SliderX", &particle_emissor_initial_velocity.x, -1000.0f, 1000.0f, "%.3f", 0);
			igText("Speed Y:");
			igSliderFloat("SliderY", &particle_emissor_initial_velocity.y, -1000.0f, 1000.0f, "%.3f", 0);
			igText("Particle Size:");
			igSliderFloat("Particle Size", &particle_emissor_base_radious, 2.0f, 50.0f, "%.3f", 0);
			igEnd();
		}
	}
	draw_start_stop_controls();
}

static SpacePartitionCell create_mega_cell(int celX, int celY)
{
	SpacePartitionCell megaCell = {0};
	for(int j = celY - 1; j <= celY + 1; j++)
	{
		for(int i = celX - 1; i <= celX + 1; i++)
		{
			SpacePartitionCell cell = space_partition_grid[i][j];
			for(int k = 0; k < cell.particleCount; k++)
			{
				megaCell.particles[megaCell.particleCount++] =  cell.particles[k];
			}
		}
	}
	return megaCell;
}

static void delete_offscreen_particles()
{
	for(int i = 0; i < num_particles; i++)
	{
		if(!particles[i].enabled) continue;

		int window_padding = 500;

		if(particles[i].position.x < -window_padding || particles[i].position.x > screen_size.x + window_padding || particles[i].position.y > screen_size.y + window_padding)
		{
			delete_particle(i);
		}
	}
}

static void constraint_user_particles_to_screen()
{
	for(int i = 0; i < num_particles; i++)
	{
		if(!particles[i].enabled) continue;
		if(!particles[i].user_created) continue;

		float prevx = particles[i].position.x;
		particles[i].position.x = MIN(MAX(5 + particles[i].radious, particles[i].position.x), screen_size.x - 5 - particles[i].radious);

		if(prevx != particles[i].position.x)
			particles[i].prevPosition.x = particles[i].position.x;


		float prevy = particles[i].position.y;	
		particles[i].position.y = MIN(MAX(5 + particles[i].radious, particles[i].position.y), screen_size.y - 5 - particles[i].radious);

		if(prevy != particles[i].position.y)
			particles[i].prevPosition.y = particles[i].position.y;
	}
}

static void constraint_particles_to_screen()
{
	for(int i = 0; i < num_particles; i++)
	{
		if(!particles[i].enabled) continue;
		float prevx = particles[i].position.x;
		particles[i].position.x = MIN(MAX(5 + particles[i].radious, particles[i].position.x), screen_size.x - 5 - particles[i].radious);

		if(prevx != particles[i].position.x)
			particles[i].prevPosition.x = particles[i].position.x;


		float prevy = particles[i].position.y;	
		particles[i].position.y = MIN(MAX(5 + particles[i].radious, particles[i].position.y), screen_size.y - 5 - particles[i].radious);

		if(prevy != particles[i].position.y)
			particles[i].prevPosition.y = particles[i].position.y;
	}
}

static void enforce_collisions_using_space_partitioning()
{
	int matrixWidth = screen_size.x / cellSize;
	int matrixHeight = screen_size.y / cellSize;
	
	reset_partition_grid();
	build_partition_grid();

	for (int i = 0; i < matrixWidth; i++)
	{
		for (int j = 0; j < matrixHeight; j++)
		{
			SpacePartitionCell *cell = &space_partition_grid[i][j];

			vt_enforce_colision_in_space_partition_cell(particles, *cell);

			const int neighbors[3][2] = {
				{1, 0}, {0, 1}, {1, 1}};

			for (int n = 0; n < 3; n++)
			{
				int ni = i + neighbors[n][0];
				int nj = j + neighbors[n][1];

				if (ni >= matrixWidth || nj >= matrixHeight)
					continue;

				SpacePartitionCell *neighbor = &space_partition_grid[ni][nj];

				for (int a = 0; a < cell->particleCount; a++)
				{
					for (int b = 0; b < neighbor->particleCount; b++)
					{
						int idxA = cell->particles[a];
						int idxB = neighbor->particles[b];
						vt_resolve_particle_collision(&particles[idxA], &particles[idxB]);
					}
				}
			}
		}
	}
}

static void process_state_running()
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);

	float delta_time = igGetIO()->DeltaTime;
	
	update_energy_display(particles, links, num_particles, num_links, delta_time);

	static float elapsed_time = 0;

	elapsed_time += delta_time;

	if(destroy_particels_outside_screen) {
		delete_offscreen_particles();
	}
	
	vt_enforce_far_constraints(particles, num_particles, far_constraints, num_far_constraints);
	vt_enforce_near_constraints(particles, num_particles, near_constraints, num_near_constraints);
	vt_enforce_link_constraints(particles, links, num_links);

	if(last_particle_added == -1 || (ig_vec2_length(ig_vec2_diff(particles[last_particle_added].position, emissor_position)) > particles[last_particle_added].radious * 2 && (num_particles - num_free_particles) < MAX_PARTICLES && inject_particles))
	{
		float rad = particle_emissor_base_radious + sin(elapsed_time);
		Particle new_particle = {emissor_position, {emissor_position.x + particle_emissor_initial_velocity.x * delta_time, emissor_position.y - particle_emissor_initial_velocity.y * delta_time}, {0, 0}, rad, rad, color_verde};
		add_particle(new_particle, false);
	}
	
	if(constrain_to_screen)
	{
		constraint_particles_to_screen();
	}

	constraint_user_particles_to_screen();

	vt_accelerate_particles(particles, num_particles, (ImVec2){0, 250.0});

	for(int i = 0; i < num_engines; i++)
	{
		engines[i].strength = motor_torque;
		gear_apply_motor_force(engines[i]);
	}


	vt_update_particles(particles, num_particles, delta_time);
	if(!space_partitioning)
	{
		for(int i = 0; i < SUB_CYCLES; i++)
		{
			vt_enforce_collision_between_particles(particles, num_particles);
		}
	} else {
		enforce_collisions_using_space_partitioning();
	}

	if(developer_mode)
	{
		igSetNextWindowPos((ImVec2) {529,400}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
		igSetNextWindowSize((ImVec2) {270,105}, ImGuiCond_FirstUseEver);
		igBegin("Motor Torque", NULL, ImGuiWindowFlags_AlwaysAutoResize);
		igSliderFloat("Slider", &motor_torque, 25.0f, 500.0f, "%.3f", 0);
		igEnd();
	}


}

static void process_state_creating_fixed_size_gear(bool fixed, bool is_engine)
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum creatingParticleState = SUBSTATE_READY;
	static ImVec2 center;

	Engine engine = {.strength = 250., .num_particles = 0};

	static float radious = 60;
	{
		bool open = false;
		igSetNextWindowPos((ImVec2) {529,400}, ImGuiCond_FirstUseEver, (ImVec2){0, 0});
		igSetNextWindowSize((ImVec2) {270,105}, ImGuiCond_FirstUseEver);
		igBegin("Generator Size", &open, ImGuiWindowFlags_AlwaysAutoResize);
		igSliderFloat("Slider", &radious, 25.0f, 200.0f, "%.3f", 0);
		igEnd();
	}
	
	if(igIsAnyItemActive()) return;

	switch(creatingParticleState)
	{
		case SUBSTATE_READY:
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				creatingParticleState = SUBSTATE_DRAWING;
				center = mouse_pos;
			}
			break;
		case SUBSTATE_DRAWING:
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddCircle(draw_list, mouse_pos, radious, color_rojo, PARTICLE_FACES, 1);
			} else 
			{
				creatingParticleState = SUBSTATE_READY;
				if(radious < 2)
				{
					soundPlaySfx(sound, SFX_HURT);
					return;
				}
				soundPlaySfx(sound, SFX_COIN);
				center = mouse_pos;

				int gear_teeth_max = 30;
				int num_teeth = 0;
				int gear_teeth_ids[gear_teeth_max];

				// center
				Particle centerP = {center, center, {0, 0}, fixed?-1:10, 10, color_rojo};
				
				engine.particle_id_center = add_particle(centerP, true);

				int center_particle_id = last_particle_added;

				int sides = 6;
			
				for(int i = 0; i < sides; i++)
				{
					float angle = 2 * PI * i / sides + PI / 2.;
					float x = sinf(angle) * radious;
					float y = cosf(angle) * radious;
			
					ImVec2 particle_position = {center.x + x, center.y + y};

					Particle newParticle = {particle_position, particle_position, {0, 0}, 10, 10, create_rgb_color(255, 0, 255, 255)};
					engine.particles_border[engine.num_particles++] = add_particle(newParticle, true);
					links[num_links++] = vt_create_link_constraint(last_particle_added, center_particle_id, particles);
					gear_teeth_ids[num_teeth++] = last_particle_added;
				}

				for(int i = 0; i < num_teeth - 1; i++)
				{
					links[num_links++] = vt_create_link_constraint(gear_teeth_ids[i], gear_teeth_ids[i + 1], particles);
				}

				links[num_links++] = vt_create_link_constraint(gear_teeth_ids[0], gear_teeth_ids[num_teeth - 1], particles);
				if(is_engine)
				{
					engines[num_engines++] = engine;
				}
			}
			break;
	}
}

static void process_state_creating_gear(bool fixed, bool is_engine)
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum creatingParticleState = SUBSTATE_READY;
	static ImVec2 center;

	if(igIsAnyItemActive()) return;

	Engine engine = {.strength = 250., .num_particles = 0};

	switch(creatingParticleState)
	{
		case SUBSTATE_READY:
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				creatingParticleState = SUBSTATE_DRAWING;
				center = mouse_pos;
				soundPlaySfx(sound, SFX_JUMP);
			}
			break;
		case SUBSTATE_DRAWING:
			float radious = ig_vec2_length(ig_vec2_diff(center, mouse_pos));
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddCircle(draw_list, center, radious, color_rojo, PARTICLE_FACES, 1);
			} else 
			{
				creatingParticleState = SUBSTATE_READY;
				if(radious < 2)
				{
					soundPlaySfx(sound, SFX_HURT);
				}

				soundPlaySfx(sound, SFX_COIN);

				int gear_teeth_max = 30;
				int num_teeth = 0;
				int gear_teeth_ids[gear_teeth_max];

				// center
				Particle centerP = {center, center, {0, 0}, fixed?-1:10, 10, color_rojo};
				engine.particle_id_center = add_particle(centerP, true);
				
				int center_particle_id = last_particle_added;

				int sides = 6;
			
				for(int i = 0; i < sides; i++)
				{
					float angle = 2 * PI * i / sides;
					float x = sinf(angle) * radious;
					float y = cosf(angle) * radious;
			
					ImVec2 particle_position = {center.x + x, center.y + y};

					Particle newParticle = {particle_position, particle_position, {0, 0}, 10, 10, color_purpura};
					engine.particles_border[engine.num_particles++] = add_particle(newParticle, true);
					links[num_links++] = vt_create_link_constraint(last_particle_added, center_particle_id, particles);
					gear_teeth_ids[num_teeth++] = last_particle_added;
				}

				for(int i = 0; i < num_teeth - 1; i++)
				{
					links[num_links++] = vt_create_link_constraint(gear_teeth_ids[i], gear_teeth_ids[i + 1], particles);
				}

				links[num_links++] = vt_create_link_constraint(gear_teeth_ids[0], gear_teeth_ids[num_teeth - 1], particles);
				if(is_engine)
				{
					engines[num_engines++] = engine;
				}
			}
			break;
	}
}

static void process_state_creating_fixed_particle(void)
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum creatingParticleState = SUBSTATE_READY;
	static ImVec2 center;

	if(igIsAnyItemActive()) return;

	switch(creatingParticleState)
	{
		case SUBSTATE_READY:
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				creatingParticleState = SUBSTATE_DRAWING;
				center = mouse_pos;
			}
			break;
		case SUBSTATE_DRAWING:
			float radious = ig_vec2_length(ig_vec2_diff(center, mouse_pos));
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddCircle(draw_list, center, radious, color_rojo, PARTICLE_FACES, 1);
			} else 
			{
				soundPlaySfx(sound, SFX_COIN);
				creatingParticleState = SUBSTATE_READY;
				if(radious < 2) return;
				Particle newParticle = {center, center, {0, 0}, -1, radious, color_rojo};
				add_particle(newParticle, true);
			}
			break;
	}
}

static void process_state_creating_particle(void)
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum creatingParticleState = SUBSTATE_READY;
	static ImVec2 center;

	if(igIsAnyItemActive()) return;

	switch(creatingParticleState)
	{
		case SUBSTATE_READY:
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				creatingParticleState = SUBSTATE_DRAWING;
				center = mouse_pos;
				soundPlaySfx(sound, SFX_JUMP);
			}
			break;
		case SUBSTATE_DRAWING:
			float radious = ig_vec2_length(ig_vec2_diff(center, mouse_pos));
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddCircle(draw_list, center, radious, color_rojo, PARTICLE_FACES, 1);
			} else 
			{
				soundPlaySfx(sound, SFX_COIN);
				creatingParticleState = SUBSTATE_READY;
				if(radious < 2) return;
				Particle newParticle = {center, center, {0, 0}, radious, radious, color_verde};
				add_particle(newParticle, true);
			}
			break;
	}
}

static void process_state_creating_standard_particle(void)
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum creatingParticleState = SUBSTATE_READY;
	static ImVec2 center;

	if(igIsAnyItemActive()) return;

	switch(creatingParticleState)
	{
		case SUBSTATE_READY:
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				creatingParticleState = SUBSTATE_DRAWING;
				center = mouse_pos;
			}
			break;
		case SUBSTATE_DRAWING:
			float radious = ig_vec2_length(ig_vec2_diff(center, mouse_pos));
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddCircle(draw_list, mouse_pos, 10, color_rojo, PARTICLE_FACES, 1);
			} else 
			{
				soundPlaySfx(sound, SFX_COIN);
				creatingParticleState = SUBSTATE_READY;
				Particle newParticle = {mouse_pos, mouse_pos, {0, 0}, 10, 10, color_verde};
				add_particle(newParticle, true);
			}
			break;
	}
}

static void draw_particles()
{
	for(int i = 0; i < num_particles; i++)
	{
		if(!particles[i].enabled) continue;
		ImDrawList_AddCircleFilled(draw_list, particles[i].position, particles[i].radious, particles[i].color, PARTICLE_FACES);
	}
}

static void draw_near_constraints()
{
	for(int i = 0; i < num_near_constraints; i++)
	{
		ImDrawList_AddCircle(draw_list, near_constraints[i].center, near_constraints[i].length, color_amarillo, CONSTRAINT_FACES, 1);
	}
}

static void draw_far_constraints()
{
	for(int i = 0; i < num_far_constraints; i++)
	{
		ImDrawList_AddCircle(draw_list, far_constraints[i].center, far_constraints[i].length, color_rojo, CONSTRAINT_FACES, 1);
	}
}

static void process_state_creating_near_constraint()
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum creatingNearConstraintState = SUBSTATE_READY;
	static ImVec2 center;

	if(igIsAnyItemActive()) return;

	switch(creatingNearConstraintState)
	{
		case SUBSTATE_READY:
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				creatingNearConstraintState = SUBSTATE_DRAWING;
				center = mouse_pos;
				soundPlaySfx(sound, SFX_JUMP);
			}
			break;
		case SUBSTATE_DRAWING:
			float radious = ig_vec2_length(ig_vec2_diff(center, mouse_pos));
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddCircle(draw_list, center, radious, color_rojo, CONSTRAINT_FACES, 1);
			} else 
			{
				soundPlaySfx(sound, SFX_COIN);
				creatingNearConstraintState = SUBSTATE_READY;
				if(radious < 2) return;
				UnidirectionalConstraint newNearConstraint = {center, radious};
				near_constraints[num_near_constraints++] = newNearConstraint;
			}
			break;
	}
}

static void process_state_creating_far_constraint()
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum creatingFarConstraintState = SUBSTATE_READY;
	static ImVec2 center;

	if(igIsAnyItemActive()) return;

	switch(creatingFarConstraintState)
	{
		case SUBSTATE_READY:
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				creatingFarConstraintState = SUBSTATE_DRAWING;
				center = mouse_pos;
			}
			break;
		case SUBSTATE_DRAWING:
			float radious = ig_vec2_length(ig_vec2_diff(center, mouse_pos));
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddCircle(draw_list, center, radious, color_rojo, CONSTRAINT_FACES, 1);
			} else 
			{
				creatingFarConstraintState = SUBSTATE_READY;
				if(radious < 2) return;
				UnidirectionalConstraint newFarConstraint = {center, radious};
				far_constraints[num_far_constraints++] = newFarConstraint;
			}
			break;
	}
}

void draw_link_constraints()
{
	for(int i = 0; i < num_links; i++)
	{
		if(!links[i].enabled) continue;
		ImDrawList_AddLine(draw_list, particles[links[i].particleA].position, particles[links[i].particleB].position, color_gris, 2);
	}
}

void highlight_particle(int particle_index)
{
	ImU32 color = get_some_color();
	Particle p = particles[particle_index];
	ImDrawList_AddCircle(draw_list, p.position, p.radious * 1.2, color, PARTICLE_FACES, 4);
}

void process_state_deleting_particle()
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum subState = SUBSTATE_READY;
	static ImVec2 center;
	static int particle = -1;

	if(igIsAnyItemActive()) return;
	particle = get_particle_under_the_mouse();
	
	highlight_particle(particle);
	
	if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
	{
		if(particle != -1)
		{
			particles[particle].enabled = false;
			soundPlaySfx(sound, SFX_HURT);
			vt_enforce_link_constraints(particles, links, num_links);
		}
	}
}

void process_state_creating_link_constraint()
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum subState = SUBSTATE_READY;
	static ImVec2 center;
	static int particleA = -1;

	if(igIsAnyItemActive()) return;

	switch(subState)
	{
		case SUBSTATE_READY:
			particleA = get_particle_under_the_mouse();
			highlight_particle(particleA);
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				if(particleA != -1)
				{
					subState = SUBSTATE_DRAWING;
					soundPlaySfx(sound, SFX_JUMP);
				}
			}
			break;
		case SUBSTATE_DRAWING:
			int particleB = get_particle_under_the_mouse();	
			highlight_particle(particleA);
			highlight_particle(particleB);
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddLine(draw_list, particles[particleA].position, mouse_pos, color_rojo, 1);
			} else {
				soundPlaySfx(sound, SFX_COIN);
				links[num_links++] = vt_create_link_constraint(particleA, particleB, particles);
				subState = SUBSTATE_READY;
			}
			break;
	}
}

static void draw_partition_grid()
{
	int matrixWidth = screen_size.x / cellSize;
	int matrixHeight = screen_size.y / cellSize;

	for (int i = 0; i < matrixWidth; i++)
	{
		for (int j = 0; j < matrixHeight; j++)
		{
			
			ImDrawList_AddRect(draw_list, (ImVec2){i * cellSize, j * cellSize}, (ImVec2){i * cellSize + cellSize, j * cellSize + cellSize}, color_rojo, 0, 0,  1);
			for(int k = 0; k < space_partition_grid[i][j].particleCount; k++)
			{
				ImDrawList_AddRectFilled(draw_list, (ImVec2){i * cellSize, j * cellSize}, (ImVec2){i * cellSize + cellSize, j * cellSize + cellSize}, create_rgb_color(255, 0, 255, 20), 0, 0);
			}
		}
	}

}

void process_state_won()
{
	igSetNextWindowSize((ImVec2){700, 500}, ImGuiCond_Once);
	igSetNextWindowPos((ImVec2){50, 50}, ImGuiCond_Once, (ImVec2){0, 0});

	igBegin("YOU WON!", NULL, ImGuiWindowFlags_None);

	igBeginChild_Str("ScrollRegion", (ImVec2){0, -70}, true, ImGuiWindowFlags_HorizontalScrollbar);

	igTextWrapped("CONGRATULLATIONS!!!!.\n\n"
				  "You achieved an energy conversion efficiency above 60%%!\n\n"
				"Dont forget to share your achievement using the Export Game and pasting the URL in the comments!!");

	igEndChild();

	if (igButton("Close", (ImVec2){0, 0}))
	{
		export_game();
		state = GAME_STATE_RUNNING;
	}

	igEnd();
}

static bool update()
{
	
	begin_frame();
	draw_list = igGetBackgroundDrawList_Nil();

	draw_background_effects(state == GAME_STATE_RUNNING?ioptr->DeltaTime:0);

	if(!show_how_to_play_window)
		draw_gui();

	draw_how_to_play_window();
	draw_about_window();

	switch (state)
	{
		case GAME_STATE_READY:
			break;
		case GAME_STATE_RUNNING:
			process_state_running();
			break;
		case GAME_STATE_CREATING_PARTICLE:
			process_state_creating_particle();
			break;
		case GAME_STATE_CREATING_FIXED_PARTICLE:
			process_state_creating_fixed_particle();
			break;
		case GAME_STATE_CREATING_FIXED_SIZE_GEAR:
			process_state_creating_fixed_size_gear(fixed_generators, false);
			break;
		case GAME_STATE_CREATING_GEAR:
			process_state_creating_gear(fixed_generators, false);
			break;
		case GAME_STATE_CREATING_FIXED_SIZE_ENGINE:
			process_state_creating_fixed_size_gear(fixed_generators, true);
			break;
		case GAME_STATE_CREATING_ENGINE:
			process_state_creating_gear(fixed_generators, true);
			break;
			case GAME_STATE_CREATING_STANDARD_PARTICLE:
			process_state_creating_standard_particle();
			break;
		case GAME_STATE_CREATING_FAR_CONSTRAINT:
			process_state_creating_far_constraint();
			break;
		case GAME_STATE_CREATING_NEAR_CONSTRAINT:
			process_state_creating_near_constraint();
			break;
		case GAME_STATE_CREATING_LINK_CONSTRAINT:
			process_state_creating_link_constraint();
			break;
		case GAME_STATE_WON:
			process_state_won();
			break;
		case GAME_STATE_DELETE_PARTICLE:
			process_state_deleting_particle();
			break;
	}

	if(draw_part_grid)
		draw_partition_grid();
	
	draw_near_constraints();
	draw_far_constraints();
	draw_particles();
	draw_link_constraints();

	igRender();
	ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
#ifdef IMGUI_HAS_DOCK
	if (ioptr->ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow *backup_current_window = glfwGetCurrentContext();
		igUpdatePlatformWindows();
		igRenderPlatformWindowsDefault(NULL, NULL);
		glfwMakeContextCurrent(backup_current_window);
	}
#endif
	end_frame(window);
	return !glfwWindowShouldClose(window) && !igIsKeyDown_Nil(ImGuiKey_Escape);
}

static void destroy()
{
	glfwSetWindowShouldClose(window, true);
	glfwDestroyWindow(window);
	glfwTerminate();
}

void updateWeb()
{
	update();
}

Game create_game()
{
	Game game = {
		true,
		init,
		updateWeb,
		update,
		destroy};

	return game;
}
