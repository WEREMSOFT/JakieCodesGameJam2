#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>
#include "sound.h"

extern bool show_about_window;
extern bool show_how_to_play_window;

extern bool show_energy_window;
extern bool show_emisor_properties_window;
extern Sound sound;

void draw_about_window() {
    if (!show_about_window)
        return;

    igSetNextWindowSize((ImVec2){700, 500}, ImGuiCond_Once);
    igSetNextWindowPos((ImVec2){50, 50}, ImGuiCond_Once, (ImVec2){0, 0});

    igBegin("About", &show_about_window, ImGuiWindowFlags_None);

    igBeginChild_Str("ScrollRegion", (ImVec2){0, -70}, true, ImGuiWindowFlags_HorizontalScrollbar);
    
    igTextWrapped(
		"This game was created by WeremSoft(me) for the Jakie Game Jam #2 during April 2025\n\n"
		"The game was developed in C99 compiled to webassembly using Emscripten.\n\n"
		"Libraries used include Soloud(not used), DearImgui(to draw stuff on screen), GLFW(openGL) and STB_IMG(not used)\n\n"
		"The game itself is basically a hello world for a physics engine, all graphics are made using the drawing API from dearImgui.\n\n"
		"As cool feature, besides the physics engine itself, implements a way to share the levels created by users using a url link.\n\n"
	);

    igEndChild();

    if (igButton("Close", (ImVec2){0, 0})) {
        show_about_window = false;
    }

    igEnd();
}

void draw_how_to_play_window() {
    if (!show_how_to_play_window)
        return;

	// Tamaño y posición inicial (solo si es la primera vez)
    igSetNextWindowSize((ImVec2){700, 500}, ImGuiCond_Once);
    igSetNextWindowPos((ImVec2){50, 50}, ImGuiCond_Once, (ImVec2){0, 0});

    // Abre la ventana
    igBegin("How To Play", &show_how_to_play_window, ImGuiWindowFlags_None);

    // Area de scroll
    igBeginChild_Str("ScrollRegion", (ImVec2){0, -70}, true, ImGuiWindowFlags_HorizontalScrollbar);
    
    // Texto largo
    igTextWrapped("This game is a particle-based sandbox where you build mechanical and energy structures.\n\n"

        "PARTICLES, GENERATORS, and REPULSION FIELDS are created by dragging on the canvas. "
        "The size and type depend on the length and direction of your drag gesture.\n\n"

        "- The EMITTER (only one) releases particles in a controlled way to generate energy.\n"
        "- GENERATORS absorb free kinetic energy from the free particles through collision and transmit it to the plasma globe.\n"
        "- REPULSION FIELDS push particles away within a defined radius.\n"
        "- FIXED PARTICLES are frozen in place and used as anchor points to build stable structures.\n\n"

        "The energy collected by the generators fuels the plasma globe in the background. You can customize "
        "the emitted particles properties (mass, radius, velocity) to influence how efficiently energy is captured.\n\n"

        "Be careful: too many particles—or particles with high energy—can bounce around chaotically, slowing down the generators’ efficiency.\n\n"

        "Use EDIT and RUN modes to interact or observe. Levels can be saved and shared via copy/paste. "
        "When using copy and paste, the shareable URL appears in the text box below the game.\n\n"

        "Experiment with different configurations to improve efficiency. Your goal: make the plasma globe as large as possible "
        "while maintaining an energy conversion efficiency above 60%%!");

    igEndChild();

    igCheckbox("Show ENERGY EFFICIENCY window", &show_energy_window);
    igCheckbox("Show EMITTER PROPERTIES window", &show_emisor_properties_window);

	static bool first_show = true;

    if (igButton("Close", (ImVec2){0, 0})) {
        show_how_to_play_window = false;
		if(first_show)
		{
			first_show = false;
			sound = soundCreate();
			Soloud_setGlobalVolume(sound.soloud, 1.);
			sound_play_speech(sound, SPEECH_WELLCOME);
		}
    }

    igEnd();
}
