#include "verlet.h"
#include <string.h>
#include <stdio.h>

#define MAX_PARTICLES 4000

extern int num_particles;
extern int num_links;
extern int num_near_constraints;
extern int num_engines;
extern Particle particles[];
extern LinkConstraint links[];
extern UnidirectionalConstraint near_constraints[];
extern Engine engines[];

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
                           "%d %.0f %.0f %.0f %.0f %.0f %.0f %u %d\n",
                           i,
                           p->position.x, p->position.y,
                           p->prev_position.x, p->prev_position.y,
                           p->mass, p->radious,
                           p->color,
                           p->user_created ? 1 : 0);
        ptr += written;
        out_size -= written;
    }

    written = snprintf(ptr, out_size, "CONSTRAINTS\n");
    ptr += written;
    out_size -= written;

    for (int i = 0; i < num_links; i++)
    {
        const LinkConstraint *c = &links[i];
        if (!c->enabled) continue;
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

    written = snprintf(ptr, out_size, "ENGINES\n");
    ptr += written;
    out_size -= written;

    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const Engine *e = &engines[i];
        if (e->num_particles <= 0) continue;
        written = snprintf(ptr, out_size, "%d %f %d", e->particle_id_center, e->strength, e->num_particles);
        ptr += written;
        out_size -= written;

        for (int j = 0; j < e->num_particles; j++)
        {
            written = snprintf(ptr, out_size, " %d", e->particles_border[j]);
            ptr += written;
            out_size -= written;
        }

        written = snprintf(ptr, out_size, "\n");
        ptr += written;
        out_size -= written;
    }
}

int deserialize_level(const char *input)
{
    if (strlen(input) == 0) return -1;
    num_particles = 0;
    num_links = 0;
    num_near_constraints = 0;
    num_engines = 0;

    const char *line = input;
    char buffer[256];
    int mode = 0; // 0 = looking, 1 = particles, 2 = constraints, 3 = near_constraints, 4 = emissor, 5 = engines

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
        else if (strcmp(buffer, "ENGINES") == 0)
            mode = 5;
        else if (mode == 1 && num_particles < MAX_PARTICLES)
        {
            Particle p = {0};
            int index = -1;
            int user_created_int = 0;
            sscanf(buffer, "%i %f %f %f %f %f %f %u %d",
                   &index,
                   &p.position.x, &p.position.y,
                   &p.prev_position.x, &p.prev_position.y,
                   &p.mass, &p.radious,
                   &p.color,
                   &user_created_int);
            p.enabled = true;
            p.user_created = (user_created_int != 0);
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
        else if (mode == 5 && num_engines < MAX_PARTICLES)
        {
            Engine *e = &engines[num_engines];

            char *token = strtok(buffer, " ");
            if (!token) continue;
            e->particle_id_center = atoi(token);

            token = strtok(NULL, " ");
            if (!token) continue;
            e->strength = (float)atof(token);

            token = strtok(NULL, " ");
            if (!token) continue;
            e->num_particles = atoi(token);

            for (int j = 0; j < e->num_particles; j++)
            {
                token = strtok(NULL, " ");
                if (!token) break;
                e->particles_border[j] = atoi(token);
            }

            num_engines++;
        }

        line += len;
        if (*line == '\n') line++;
    }

    return 1; // success
}

