#include "verlet.h"
#include <math.h>

typedef struct EnergyOutput
{
	float linked_particles;
	float free_particles;
} EnergyOutput;

static float calculate_angle(ImVec2 from, ImVec2 to) {
    return atan2f(to.y - from.y, to.x - from.x);
}

EnergyOutput calculate_kinetic_energy(Particle* particles, LinkConstraint* constraints, int numParticles, int numConstraints, float deltaT) {
    EnergyOutput returnValue = {0};

    int* linkCount = (int*)calloc(numParticles, sizeof(int));
    if (!linkCount) return returnValue;

    for (int i = 0; i < numConstraints; i++) {
        if (constraints[i].enabled) {
            if (constraints[i].particleA >= 0 && constraints[i].particleA < numParticles)
                linkCount[constraints[i].particleA]++;
            if (constraints[i].particleB >= 0 && constraints[i].particleB < numParticles)
                linkCount[constraints[i].particleB]++;
        }
    }

    for (int i = 0; i < numParticles; i++) {
        if (!particles[i].enabled) continue;

        ImVec2 velocity = {
            particles[i].position.x - particles[i].prev_position.x,
            particles[i].position.y - particles[i].prev_position.y
        };
        float speed = sqrtf(velocity.x * velocity.x + velocity.y * velocity.y);
        float kineticEnergy = 0.5f * particles[i].mass * speed * speed;

        if (linkCount[i] == 0) {
            returnValue.free_particles += kineticEnergy;
        } else {
            returnValue.linked_particles += kineticEnergy / (float)linkCount[i];
        }
    }

    free(linkCount);

    return returnValue;
}

#define MAX_HISTORY 300 

static float energyLinkedHistory[MAX_HISTORY] = {0};
static float energyUnlinkedHistory[MAX_HISTORY] = {0};
static float extractionEfficiencyHistory[MAX_HISTORY] = {0};
static int historyIndex = 0;
static bool historyWrapped = true;

float get_max_value(const float* data, int count) {
    float max = data[0];
    for (int i = 1; i < count; i++) {
        if (data[i] > max) {
            max = data[i];
        }
    }
    return max;
}

void draw_energy_graph(bool *close_var)
{
	int count = historyWrapped ? MAX_HISTORY : historyIndex;

	float maxLinked = get_max_value(energyLinkedHistory, count);
	float maxUnlinked = get_max_value(energyUnlinkedHistory, count);
	float scaleMax = fmaxf(maxLinked, maxUnlinked) * 1.1f;

	char title_unlinked[100] = {0};
	sprintf(title_unlinked, "Energy Available %.2f", energyUnlinkedHistory[historyIndex]);

	char title_linked[100] = {0};
	sprintf(title_linked, "Energy Collected %.2f", energyLinkedHistory[historyIndex]);

	char extraction_efficiency[100] = {0};
	sprintf(extraction_efficiency, "Extraction Efficiency %.2f", extractionEfficiencyHistory[historyIndex]);

	if (igBegin("Energy Extraction vs Time", close_var, 0))
	{
		igPlotLines_FloatPtr(
			title_linked,
			energyLinkedHistory,
			count,
			historyWrapped ? historyIndex : 0,
			NULL,
			0,
			scaleMax,
			(ImVec2){200, 30},
			sizeof(float));

		igPlotLines_FloatPtr(
			title_unlinked,
			energyUnlinkedHistory,
			count,
			historyWrapped ? historyIndex : 0,
			NULL,
			0,
			scaleMax,
			(ImVec2){200, 30},
			sizeof(float));

		igPlotLines_FloatPtr(
			extraction_efficiency,
			extractionEfficiencyHistory,
			count,
			historyWrapped ? historyIndex : 0,
			NULL,
			0,
			100,
			(ImVec2){200, 30},
			sizeof(float));
	}
	igEnd();
}

void update_energy_history(float linked, float unlinked) {
    energyLinkedHistory[historyIndex] = linked;
    energyUnlinkedHistory[historyIndex] = unlinked;
	extractionEfficiencyHistory[historyIndex] = linked / unlinked * 100.;

    historyIndex++;
    if (historyIndex >= MAX_HISTORY) {
        historyIndex = 0;
        historyWrapped = true;
    }
}

void update_energy_display(Particle* particles, LinkConstraint* constraints, int numParticles, int numConstraints, float deltaT) {
    float linked = 0.0f, unlinked = 0.0f;
    
	EnergyOutput eo = calculate_kinetic_energy(particles, constraints, numParticles, numConstraints, deltaT);
	
    update_energy_history(eo.linked_particles, eo.free_particles);
}