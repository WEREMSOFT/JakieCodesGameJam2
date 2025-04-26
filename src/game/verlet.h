#ifndef __VERLET_H__
#define __VERLET_H__

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>

#include "verlet.h"

typedef struct Particle
{
	ImVec2 position, prevPosition, acceleration;
	float mass, radious;
	ImU32 color;
	bool enabled;
	bool user_created;
} Particle;

typedef struct UnidirectionalConstraint
{
	ImVec2 center;
	float length;
} UnidirectionalConstraint;

typedef struct LinkConstraint
{
	int particleA, particleB;
	float length;
	bool enabled;
} LinkConstraint;

typedef struct SpacePartitionCell{
	uint16_t particleCount;
	uint16_t particles[500];
}SpacePartitionCell;

void vt_update_particles(Particle particles[], int particleCount, float deltaTime);
void vt_accelerate_particles(Particle particles[], int particleCount, ImVec2 acceleration);
void vt_enforce_far_constraints(Particle particles[], int numParticles, UnidirectionalConstraint farConstraints[], int numConstraints);
void vt_enforce_near_constraints(Particle particles[], int numParticles, UnidirectionalConstraint nearConstraints[], int numConstraints);
void vt_enforce_link_constraints(Particle particles[], LinkConstraint linkConstraints[], int numConstraints);
LinkConstraint vt_create_link_constraint(int particleA, int particleB, Particle particles[]);
void vt_enforce_collision_between_particles(Particle particles[], int numParticles);
void vt_resolve_particle_collision(Particle* particleA, Particle* particleB);
void vt_enforce_colision_in_space_partition_cell(Particle particles[], SpacePartitionCell spacePartitionCell);
#endif