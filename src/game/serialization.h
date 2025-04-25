#include "verlet.h"
#include <string.h>
#include <stdio.h>

#define MAX_PARTICLES 4000

extern int num_particles;
extern int num_links;
extern int num_near_constraints;
extern Particle particles[];
extern LinkConstraint links[];
extern UnidirectionalConstraint near_constraints[];

extern ImVec2 particle_emissor_initial_velocity;
extern float particle_emissor_base_radious;
extern ImVec2 emissor_position;

void serialize_level(char *out_buffer, size_t out_size)
{
	char *ptr = out_buffer;
	int written = snprintf(ptr, out_size, "PARTICLES\n");
	ptr += written;
	out_size -= written;

	for (int i = 0; i < num_particles; i++)
	{
		const Particle *p = &particles[i];
		if (!p->enabled) continue;
		written = snprintf(ptr, out_size,
						   "%d %.0f %.0f %.0f %.0f %.0f %.0f %u\n",
						   i,
						   p->position.x, p->position.y,
						   p->prevPosition.x, p->prevPosition.y,
						   p->mass, p->radious,
						   p->color);
		ptr += written;
		out_size -= written;
	}

	written = snprintf(ptr, out_size, "CONSTRAINTS\n");
	ptr += written;
	out_size -= written;

	for (int i = 0; i < num_links; i++)
	{
		const LinkConstraint *c = &links[i];
		if(!c->enabled) continue;
		written = snprintf(ptr, out_size,
						   "%d %d %.0f\n", c->particleA, c->particleB, c->length);
		ptr += written;
		out_size -= written;
	}

	written = snprintf(ptr, out_size, "NEAR_CONSTRAINTS\n");
	ptr += written;
	out_size -= written;

	for (int i = 0; i < num_near_constraints; i++)
	{
		const UnidirectionalConstraint *c = &near_constraints[i];
		written = snprintf(ptr, out_size,
						   "%.1f %.1f %.1f\n", c->center.x, c->center.y, c->length);
		ptr += written;
		out_size -= written;
	}

	written = snprintf(ptr, out_size, "EMISSOR\n");
	ptr += written;
	out_size -= written;

	written = snprintf(ptr, out_size,
					   "%.2f %.2f %.2f\n",
					   particle_emissor_initial_velocity.x,
					   particle_emissor_initial_velocity.y,
					   particle_emissor_base_radious);
	ptr += written;
	out_size -= written;
}

int deserialize_level(const char *input)
{
	if (strlen(input) == 0) return -1;
	num_particles = 0;
	num_links = 0;
	num_near_constraints = 0;

	const char *line = input;
	char buffer[256];
	int mode = 0; // 0 = looking, 1 = particles, 2 = constraints, 3 = near_constraints, 4 = emissor

	while (*line)
	{
		int len = 0;
		while (line[len] && line[len] != '\n')
			len++;
		if (len >= sizeof(buffer))
			len = sizeof(buffer) - 1;
		strncpy(buffer, line, len);
		buffer[len] = '\0';

		if (strcmp(buffer, "PARTICLES") == 0)
			mode = 1;
		else if (strcmp(buffer, "CONSTRAINTS") == 0)
			mode = 2;
		else if (strcmp(buffer, "NEAR_CONSTRAINTS") == 0)
			mode = 3;
		else if (strcmp(buffer, "EMISSOR") == 0)
			mode = 4;
		else if (mode == 1 && num_particles < MAX_PARTICLES)
		{
			Particle p = {0};
			int index = -1;
			sscanf(buffer, "%i %f %f %f %f %f %f %u",
				   &index,
				   &p.position.x, &p.position.y,
				   &p.prevPosition.x, &p.prevPosition.y,
				   &p.mass, &p.radious,
				   &p.color);
			p.enabled = true;
			particles[index] = p;
			num_particles = index + 1;
		}
		else if (mode == 2 && num_links < MAX_PARTICLES)
		{
			LinkConstraint *c = &links[num_links];
			sscanf(buffer, "%d %d %f", &c->particleA, &c->particleB, &c->length);
			c->enabled = true;
			num_links++;
		}
		else if (mode == 3 && num_near_constraints < MAX_PARTICLES)
		{
			UnidirectionalConstraint *c = &near_constraints[num_near_constraints];
			sscanf(buffer, "%f %f %f", &c->center.x, &c->center.y, &c->length);
			num_near_constraints++;
		}
		else if (mode == 4)
		{
			sscanf(buffer, "%f %f %f",
				   &particle_emissor_initial_velocity.x,
				   &particle_emissor_initial_velocity.y,
				   &particle_emissor_base_radious);
		}

		line += len;
		if (*line == '\n') line++;
	}

	return 1; // success
}