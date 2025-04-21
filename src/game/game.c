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
#include <emscripten-browser-clipboard.h>
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

#define PARTICLE_FACES 30
#define CONSTRAINT_FACES 64
#define SUB_CYCLES 1
#define THREAD_COUNT 16

static GLFWwindow *window;
static ImGuiIO *ioptr;

static int num_particles = 0;
static int last_particle_added = -1;
static int num_far_constraints = 0;
static int num_near_constraints = 0;
static int num_links = 0;
static int num_free_particles;
#define MAX_PARTICLES 4000

#ifndef PI
#define PI 3.1415926535897932384626433832795f
#endif

// pthreads
static pthread_t threads[THREAD_COUNT] = {0};
static pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t cond_start = PTHREAD_COND_INITIALIZER;
static pthread_cond_t cond_done = PTHREAD_COND_INITIALIZER;
static bool running = false;
static int done_counter = 0;

static LinkConstraint links[MAX_PARTICLES];
static Particle particles[MAX_PARTICLES];
static int free_particles_queue[MAX_PARTICLES] = {0};
static UnidirectionalConstraint far_constraints[MAX_PARTICLES];
static UnidirectionalConstraint near_constraints[MAX_PARTICLES];
static GameStateEnum state = GAME_STATE_RUNNING;
static ImDrawList *draw_list;
static ImVec2 screen_size = {};
#define MAX_LEVEL_TEXT 100000
static char levelTextBuffer[MAX_LEVEL_TEXT] = "";

static SpacePartitionCell space_partition_grid[50][50] = {0};

int cellSize = 20;

static ImU32 color_rojo;
static ImU32 color_blanco;
static ImU32 color_verde;

bool space_partitioning = false;
bool inject_particles = true;
bool constrain_to_screen = true;
bool destroy_particels_outside_screen = true;
bool draw_part_grid = false;
bool show_import_window = false;
bool show_import_modal = false;

static ImU32 create_rgb_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	return (r) | (g << 8) | (b << 16) | (a << 24);
}

static ImU32 get_random_color()
{
	static float phase_r = PI, phase_g = 2 * PI, phase_b = 3 * PI;

	uint8_t r = floor(sinf(phase_r+=.1) * 255);
	uint8_t g = floor(sinf(phase_g+=.1) * 255);
	uint8_t b = floor(sinf(phase_b+=.1) * 255);

	return create_rgb_color(r, g, b, 255);
}

static void init_colors()
{
	color_rojo = create_rgb_color(255, 0, 0, 255);
	color_blanco = create_rgb_color(255, 255, 255, 255);
	color_verde = create_rgb_color(0, 255, 0, 255);
}

static void add_particle(Particle particle)
{
	particle.enabled = true;
	if(num_free_particles>0)
	{
		int free_particle = free_particles_queue[--num_free_particles];
		last_particle_added = free_particle;
		particles[free_particle] = particle;
		return;
	}
	last_particle_added = num_particles;
	particles[num_particles++] = particle;
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
		float dist = ig_vec2_length(ig_vec2_diff(particles[i].position, mouse_pos));
		if(dist < particles[i].radious) return i;
	}

	return -1;
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

static void* process_space_partition_grid_slice(void *arg)
{
	int thread_id = *(int *)arg;

	int matrixWidth = screen_size.x / cellSize;
	int matrixHeight = screen_size.y / cellSize;
	
	int cols_per_thread = matrixWidth / THREAD_COUNT;
	int extra = matrixWidth % THREAD_COUNT;
	
	int start_col = thread_id * cols_per_thread + (thread_id < extra ? thread_id : extra);
	int cols_this_thread = cols_per_thread + (thread_id < extra ? 1 : 0);
	int end_col = start_col + cols_this_thread;
	

	while(true)
	{
		pthread_mutex_lock(&mutex);
		while(!running)
			pthread_cond_wait(&cond_start, &mutex);

		pthread_mutex_unlock(&mutex);

		for(int c = 0; c < SUB_CYCLES; c++)
		{
			for(int j = 0; j < matrixHeight; j++)
			{
				for(int i = start_col; i < end_col; i++)
				{
					SpacePartitionCell *cell = &space_partition_grid[i][j];
					vt_enforce_colision_in_space_partition_cell(particles, *cell);

					const int neighbors[3][2] = {{1, 0}, {0, 1}, {1, 1}};

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

		pthread_mutex_lock(&mutex);
		done_counter++;
		if(done_counter == THREAD_COUNT)
		{
			pthread_cond_signal(&cond_done);
		}
		pthread_mutex_unlock(&mutex);
	}
	return NULL;
}

static void start_processing()
{
	reset_partition_grid();
	build_partition_grid();
	pthread_mutex_lock(&mutex);
	running = true;
	done_counter = 0;
	pthread_cond_broadcast(&cond_start);
	pthread_mutex_unlock(&mutex);
}

static void wait_for_completion()
{
	pthread_mutex_lock(&mutex);
	
	while(done_counter < THREAD_COUNT)
	{
		pthread_cond_wait(&cond_done, &mutex);
	}

	running = false;

	pthread_mutex_unlock(&mutex);
}


static void init_threads()
{
	int matrixWidth = screen_size.x / cellSize;
	int matrixHeight = screen_size.y / cellSize;

	int start = matrixWidth / THREAD_COUNT;

	static int ids[THREAD_COUNT];
	for(int i = 0; i < THREAD_COUNT; i++)
	{
		ids[i] = i;
		pthread_create(&threads[i], NULL, process_space_partition_grid_slice, &ids[i]);
	}
}

static void init(int width, int height, int window_scale, bool full_screen)
{
	screen_size.x = width;
	screen_size.y = height;
	printf("initializing game...\n");
	window = glfwCreateAndInitWindow(width, height, window_scale, full_screen);
	glfwSwapInterval(1); 
	ioptr = init_dear_imgui(window);
	image = LoadTexture("assets/DinoSprites - doux.png");
	particles[0] = (Particle){(ImVec2){300, 20}, (ImVec2){300, 20}, (ImVec2){0, 0}, 1.};
	init_colors();
	ioptr->BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;
	
	#ifdef __EMSCRIPTEN__
	imgui_clipboard_init();
	#endif
	
	init_threads();
}

void serialize_level(char *out_buffer, size_t out_size)
{
	char *ptr = out_buffer;
	int written = snprintf(ptr, out_size, "PARTICLES\n");
	ptr += written;
	out_size -= written;

	for (int i = 0; i < num_particles; i++)
	{
		const Particle *p = &particles[i];
		written = snprintf(ptr, out_size,
						   "%.3f %.3f %.3f %.3f %.3f %.3f %.3f %.3f %u %d\n",
						   p->position.x, p->position.y,
						   p->prevPosition.x, p->prevPosition.y,
						   p->acceleration.x, p->acceleration.y,
						   p->mass, p->radious,
						   p->color, p->enabled ? 1 : 0);
		ptr += written;
		out_size -= written;
	}

	written = snprintf(ptr, out_size, "CONSTRAINTS\n");
	ptr += written;
	out_size -= written;

	for (int i = 0; i < num_links; i++)
	{
		const LinkConstraint *c = &links[i];
		written = snprintf(ptr, out_size,
						   "%d %d %.3f\n", c->particleA, c->particleB, c->length);
		ptr += written;
		out_size -= written;
	}
}

int deserialize_level(const char *input)
{
	num_particles = 0;
	num_links = 0;

	const char *line = input;
	char buffer[256];
	int mode = 0; // 0 = looking, 1 = particles, 2 = constraints

	while (*line)
	{
		// Read line
		int len = 0;
		while (line[len] && line[len] != '\n')
			len++;
		if (len >= sizeof(buffer))
			len = sizeof(buffer) - 1;
		strncpy(buffer, line, len);
		buffer[len] = '\0';

		if (strcmp(buffer, "PARTICLES") == 0)
		{
			mode = 1;
		}
		else if (strcmp(buffer, "CONSTRAINTS") == 0)
		{
			mode = 2;
		}
		else if (mode == 1 && num_particles < MAX_PARTICLES)
		{
			Particle *p = &particles[num_particles];
			int enabled_int;
			sscanf(buffer, "%f %f %f %f %f %f %f %f %u %d",
				   &p->position.x, &p->position.y,
				   &p->prevPosition.x, &p->prevPosition.y,
				   &p->acceleration.x, &p->acceleration.y,
				   &p->mass, &p->radious,
				   &p->color, &enabled_int);
			p->enabled = (enabled_int != 0);
			num_particles++;
		}
		else if (mode == 2 && num_links < MAX_PARTICLES)
		{
			LinkConstraint *c = &links[num_links];
			sscanf(buffer, "%d %d %f", &c->particleA, &c->particleB, &c->length);
			num_links++;
		}

		line += len;
		if (*line == '\n')
			line++;
	}

	return 1; // success
}

static void export_game(void)
{
	serialize_level(levelTextBuffer, MAX_LEVEL_TEXT);
	
	if (igBeginPopupModal("Import", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		igText("Paste here:");
		igInputTextMultiline("##LevelInput", levelTextBuffer, MAX_LEVEL_TEXT,
							 (ImVec2){500, 300}, ImGuiInputTextFlags_AllowTabInput, NULL, NULL);

		if (igButton("Ok", (ImVec2){0, 0}))
		{
			if (!deserialize_level(levelTextBuffer))
			{
				printf("Error deserializing game\n");
			}
			igCloseCurrentPopup();
			state = GAME_STATE_RUNNING;
			show_import_modal = false;
		}

		igEndPopup();
	}
 	igOpenPopup_Str("Import", 0);
}

static void import_game(void)
{
	if (igBeginPopupModal("Import", NULL, ImGuiWindowFlags_AlwaysAutoResize))
	{
		igText("Paste here:");
		igInputTextMultiline("##LevelInput", levelTextBuffer, MAX_LEVEL_TEXT,
							 (ImVec2){500, 300}, ImGuiInputTextFlags_AllowTabInput, NULL, NULL);

		if (igButton("Load", (ImVec2){0, 0}))
		{

			if (!deserialize_level(levelTextBuffer))
			{
				printf("Error al parsear json\n");
			}
			igCloseCurrentPopup();
			state = GAME_STATE_RUNNING;
			show_import_modal = false;
		}

		igEndPopup();
	}
 	igOpenPopup_Str("Import", 0);
}

static bool draw_gui()
{
	if (igBeginMainMenuBar())
	{
		if (igBeginMenu("Edit", true))
		{
			state = GAME_STATE_READY;
			if (igMenuItem_Bool("Add Particle", "", false, true))
			{
				state = GAME_STATE_CREATING_PARTICLE;
			}
			if (igMenuItem_Bool("Add Fixed Particle", "", false, true))
			{
				state = GAME_STATE_CREATING_FIXED_PARTICLE;
			}
			if (igMenuItem_Bool("Add Standard Particle", "", false, true))
			{
				state = GAME_STATE_CREATING_STANDARD_PARTICLE;
			}
			if (igMenuItem_Bool("Add Far Constraint", "", false, true))
			{
				state = GAME_STATE_CREATING_FAR_CONSTRAINT;
			}
			if (igMenuItem_Bool("Add Near Constraint", "", false, true))
			{
				state = GAME_STATE_CREATING_NEAR_CONSTRAINT;
			}
			if (igMenuItem_Bool("Add Link Constraint", "", false, true))
			{
				state = GAME_STATE_CREATING_LINK_CONSTRAINT;
			}
			if (igMenuItem_Bool("Add Fixed Size Gear", "", false, true))
			{
				state = GAME_STATE_CREATING_FIXED_SIZE_GEAR;
			}
			if (igMenuItem_Bool("Add Gear", "", false, true))
			{
				state = GAME_STATE_CREATING_GEAR;
			}
			if (igMenuItem_Bool("Run", "", false, true))
			{
				state = GAME_STATE_RUNNING;
			}
			if (igMenuItem_Bool("Reset", "", false, true))
			{
				num_free_particles =  num_particles = num_near_constraints =  num_far_constraints = num_links = 0;
				last_particle_added = -1;
				
			}
			igSeparator();
			if (igMenuItem_BoolPtr("Constrain Particles To Screen", "", &constrain_to_screen, true));
			if (igMenuItem_BoolPtr("Destroy Particles Out Of Screen", "", &destroy_particels_outside_screen, true));
			if (igMenuItem_BoolPtr("Space Partitioning", "", &space_partitioning, true));
			if (igMenuItem_BoolPtr("Inject Particles", "", &inject_particles, true));
			if (igMenuItem_BoolPtr("Draw Partition Grid", "", &draw_part_grid, true));
			igSeparator();
			if (igMenuItem_Bool("Export Game", "", &draw_part_grid, true))
			{
				export_game();
			}
			if (igMenuItem_BoolPtr("Import Game", "", &show_import_modal, true));
			igSeparator();

			if (igMenuItem_Bool("Copy", "CTRL+C", false, true))
			{
			}
			if (igMenuItem_Bool("Paste", "CTRL+V", false, true))
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
	}
	
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

		if(particles[i].position.x < 0 || particles[i].position.x > screen_size.x || particles[i].position.y > screen_size.y)
		{
			delete_particle(i);
		}
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

	static float elapsed_time = 0;

	elapsed_time += delta_time;

	static ImVec2 initial_velocity = { 0, 0 };
	static float base_rad = 2.;
	{
		bool open = false;
		igSetNextWindowPos((ImVec2) {500,19}, ImGuiCond_Once, (ImVec2){0, 0});
		igSetNextWindowSize((ImVec2) {270,105}, ImGuiCond_Once);
		igBegin("Particle Emisor Properties", &open, ImGuiWindowFlags_AlwaysAutoResize);
		igText("Speed X:");
		igSliderFloat("SliderX", &initial_velocity.x, -1000.0f, 1000.0f, "%.3f", 0);
		igText("Speed Y:");
		igSliderFloat("SliderY", &initial_velocity.y, -1000.0f, 1000.0f, "%.3f", 0);
		igText("Particle Size:");
		igSliderFloat("Particle Size", &base_rad, 2.0f, 3.0f, "%.3f", 0);
		igEnd();
	}

	ImVec2 emisor = {.x = screen_size.x/2 + 30, .y = screen_size.y/2-200};

	if(last_particle_added == -1 || (ig_vec2_length(ig_vec2_diff(particles[last_particle_added].position, emisor)) > particles[last_particle_added].radious * 2 && (num_particles - num_free_particles) < MAX_PARTICLES && inject_particles))
	{
		float rad = base_rad + sin(elapsed_time);
		Particle new_particle = {emisor, {emisor.x + initial_velocity.x * delta_time, emisor.y - initial_velocity.y * delta_time}, {0, 0}, 1, rad, get_random_color()};
		add_particle(new_particle);
	}
	
	vt_enforce_far_constraints(particles, num_particles, far_constraints, num_far_constraints);
	vt_enforce_near_constraints(particles, num_particles, near_constraints, num_near_constraints);
	vt_enforce_link_constraints(particles, links, num_links);
	
	if(constrain_to_screen)
	{
		constraint_particles_to_screen();
	} 
	
	if(destroy_particels_outside_screen) {
		delete_offscreen_particles();
	}

	vt_accelerate_particles(particles, num_particles, (ImVec2){0, 250.0});
	vt_update_particles(particles, num_particles, delta_time);
	if(!space_partitioning)
	{
		for(int i = 0; i < SUB_CYCLES; i++)
		{
			vt_enforce_collision_between_particles(particles, num_particles);
		}
	} else {
		start_processing();
		wait_for_completion();
	}

}

static void process_state_creating_fixed_size_gear(void)
{
	ImVec2 mouse_pos;
	igGetMousePos(&mouse_pos);
	static GenericSubstateEnum creatingParticleState = SUBSTATE_READY;
	static ImVec2 center;

	static float radious = 60;
	{
		bool open = false;
		igSetNextWindowPos((ImVec2) {529,19}, ImGuiCond_Once, (ImVec2){0, 0});
		igSetNextWindowSize((ImVec2) {270,105}, ImGuiCond_Once);
		igBegin("Gear Size", &open, ImGuiWindowFlags_AlwaysAutoResize);
		igText("Ajustá el valor:");
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
				if(radious < 2) return;

				center = mouse_pos;

				int gear_teeth_max = 30;
				int num_teeth = 0;
				int gear_teeth_ids[gear_teeth_max];

				// center
				Particle centerP = {center, center, {0, 0}, -1, 10, get_random_color()};
				add_particle(centerP);
				
				int center_particle_id = last_particle_added;

				int sides = 6;
			
				for(int i = 0; i < sides; i++)
				{
					float angle = 2 * PI * i / sides;
					float x = sinf(angle) * radious;
					float y = cosf(angle) * radious;
			
					ImVec2 particle_position = {center.x + x, center.y + y};

					Particle newParticle = {particle_position, particle_position, {0, 0}, 10, 10, get_random_color()};
					add_particle(newParticle);
					links[num_links++] = vt_create_link_constraint(last_particle_added, center_particle_id, particles);
					gear_teeth_ids[num_teeth++] = last_particle_added;
				}

				for(int i = 0; i < num_teeth - 1; i++)
				{
					links[num_links++] = vt_create_link_constraint(gear_teeth_ids[i], gear_teeth_ids[i + 1], particles);
				}

				links[num_links++] = vt_create_link_constraint(gear_teeth_ids[0], gear_teeth_ids[num_teeth - 1], particles);

			}
			break;
	}
}

static void process_state_creating_gear(void)
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
				creatingParticleState = SUBSTATE_READY;
				if(radious < 2) return;

				int gear_teeth_max = 30;
				int num_teeth = 0;
				int gear_teeth_ids[gear_teeth_max];

				// center
				Particle centerP = {center, center, {0, 0}, -1, 10, get_random_color()};
				add_particle(centerP);
				
				int center_particle_id = last_particle_added;

				int sides = 6;
			
				for(int i = 0; i < sides; i++)
				{
					float angle = 2 * PI * i / sides;
					float x = sinf(angle) * radious;
					float y = cosf(angle) * radious;
			
					ImVec2 particle_position = {center.x + x, center.y + y};

					Particle newParticle = {particle_position, particle_position, {0, 0}, 10, 10, get_random_color()};
					add_particle(newParticle);
					links[num_links++] = vt_create_link_constraint(last_particle_added, center_particle_id, particles);
					gear_teeth_ids[num_teeth++] = last_particle_added;
				}

				for(int i = 0; i < num_teeth - 1; i++)
				{
					links[num_links++] = vt_create_link_constraint(gear_teeth_ids[i], gear_teeth_ids[i + 1], particles);
				}

				links[num_links++] = vt_create_link_constraint(gear_teeth_ids[0], gear_teeth_ids[num_teeth - 1], particles);

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
				creatingParticleState = SUBSTATE_READY;
				if(radious < 2) return;
				Particle newParticle = {center, center, {0, 0}, -1, radious, get_random_color()};
				add_particle(newParticle);
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
				if(radious < 2) return;
				Particle newParticle = {center, center, {0, 0}, radious, radious, get_random_color()};
				add_particle(newParticle);
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
				creatingParticleState = SUBSTATE_READY;
				Particle newParticle = {mouse_pos, mouse_pos, {0, 0}, 10, 10, get_random_color()};
				add_particle(newParticle);
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
		ImDrawList_AddCircle(draw_list, near_constraints[i].center, near_constraints[i].length, color_blanco, CONSTRAINT_FACES, 1);
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
			}
			break;
		case SUBSTATE_DRAWING:
			float radious = ig_vec2_length(ig_vec2_diff(center, mouse_pos));
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddCircle(draw_list, center, radious, color_rojo, CONSTRAINT_FACES, 1);
			} else 
			{
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
		ImDrawList_AddLine(draw_list, particles[links[i].particleA].position, particles[links[i].particleB].position, color_blanco, 2);
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
			if(igIsMouseClicked_Bool(ImGuiMouseButton_Left, false))
			{
				particleA = get_particle_under_the_mouse();
				if(particleA != -1)
				{
					subState = SUBSTATE_DRAWING;
				}
			}
			break;
		case SUBSTATE_DRAWING:
			if(igIsMouseDown_Nil(ImGuiMouseButton_Left))
			{
				ImDrawList_AddLine(draw_list, particles[particleA].position, mouse_pos, color_rojo, 1);
			} else {
				int particleB = get_particle_under_the_mouse();	
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

static bool update()
{
	begin_frame();
	draw_list = igGetBackgroundDrawList_Nil();
	draw_gui();

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
			process_state_creating_fixed_size_gear();
			break;
		case GAME_STATE_CREATING_GEAR:
			process_state_creating_gear();
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
	}

	if(show_import_modal)
	{
		import_game();		
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
