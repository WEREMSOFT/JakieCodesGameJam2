#include "verlet.h"
#include "imgui_vector_math.h"

void vt_update_particles(Particle particles[], int particleCount, float deltaTime)
{
	float dtSqr = deltaTime * deltaTime;

	for (int i = 0; i < particleCount; i++)
	{
		Particle *p = &particles[i];
		if(!p->enabled) continue;
		if (p->mass < 0)
			continue;

		ImVec2 currentPosition = p->position;
		ImVec2 velocity = ig_vec2_diff(p->position, p->prevPosition);

		float vel_scalar = ig_vec2_length(velocity);

		p->position = ig_vec2_add(
			ig_vec2_add(p->position, velocity),
			ig_vec2_scale(p->acceleration, dtSqr)
		);

		p->prevPosition = currentPosition;

		p->acceleration = (ImVec2){0};
	}
}

void vt_accelerate_particles(Particle particles[], int particleCount, ImVec2 acceleration)
{
	for(int i = 0; i < particleCount; i++)
	{
		if(particles[i].enabled)
		{
			particles[i].acceleration = ig_vec2_add(acceleration, particles[i].acceleration);
		}
	}
}

void vt_enforce_far_constraints(Particle particles[], int numParticles, UnidirectionalConstraint farConstraints[], int numConstraints)
{
	for(int i = 0; i < numConstraints; i++)
	{
		UnidirectionalConstraint farConstraint = farConstraints[i];
		for(int j = 0; j < numParticles; j++)
		{
			ImVec2 to_obj = ig_vec2_diff(particles[j].position, farConstraint.center);

			float distance = ig_vec2_length(to_obj);
			if(distance > farConstraint.length - particles[j].radious)
			{
				ImVec2 normalVector = ig_vec2_norm(to_obj);
				ImVec2 correctionVector = ig_vec2_scale(normalVector, farConstraint.length - particles[j].radious);
				particles[j].position = ig_vec2_add(farConstraint.center, correctionVector);
			}
		}
	}
}

void vt_enforce_near_constraints(Particle particles[], int numParticles, UnidirectionalConstraint nearConstraints[], int numConstraints)
{
	for(int i = 0; i < numConstraints; i++)
	{
		UnidirectionalConstraint nearConstraint = nearConstraints[i];
		for(int j = 0; j < numParticles; j++)
		{
			Particle particle = particles[j];

			ImVec2 to_obj = ig_vec2_diff(particle.position, nearConstraint.center);

			float distance = ig_vec2_length(to_obj);
			if(distance < nearConstraint.length + particle.radious)
			{
				ImVec2 normalVector = ig_vec2_norm(to_obj);
				ImVec2 correctionVector = ig_vec2_scale(normalVector, nearConstraint.length + particle.radious);
				float correctionlength = ig_vec2_length(correctionVector);
				particle.position = ig_vec2_add(nearConstraint.center, correctionVector);
				particles[j] = particle;
			}
		}
	}
}

LinkConstraint vt_create_link_constraint(int particleA, int particleB, Particle particles[])
{
	return (LinkConstraint){.enabled = true, .particleA = particleA, .particleB = particleB, .length = ig_vec2_length(ig_vec2_diff(particles[particleA].position, particles[particleB].position))};
}

void vt_enforce_link_constraints(Particle particles[], LinkConstraint linkConstraints[], int numConstraints)
{
	for (int i = 0; i < numConstraints; i++)
	{
		if(!linkConstraints[i].enabled) continue;

		LinkConstraint constraint = linkConstraints[i];
		Particle *pA = &particles[constraint.particleA];
		Particle *pB = &particles[constraint.particleB];

		if(!(pA->enabled && pB->enabled))
		{
			linkConstraints[i].enabled = false;
			continue;
		} 
			

		ImVec2 axis = ig_vec2_diff(pA->position, pB->position);
		float dist = ig_vec2_length(axis);

		if (dist < 0.0001f)
			continue; 

		ImVec2 normal = ig_vec2_scale(axis, 1.0f / dist);
		float delta = constraint.length - dist;

		bool infA = (pA->mass < 0);
		bool infB = (pB->mass < 0);

		if (infA && infB)
		{
			continue;
		}
		else if (infA)
		{
			pB->position = ig_vec2_diff(pB->position, ig_vec2_scale(normal, delta));
		}
		else if (infB)
		{
			pA->position = ig_vec2_add(pA->position, ig_vec2_scale(normal, delta));
		}
		else
		{
			float totalMass = pA->mass + pB->mass;
			float ratioA = pB->mass / totalMass;
			float ratioB = pA->mass / totalMass;

			pA->position = ig_vec2_add(pA->position, ig_vec2_scale(normal, delta * ratioA));
			pB->position = ig_vec2_diff(pB->position, ig_vec2_scale(normal, delta * ratioB));
		}
	}
}


void vt_enforce_collision_between_particles(Particle particles[], int numParticles)
{
	for (int i = 0; i < numParticles; i++)
	{
		Particle *particleA = &particles[i];
		if(!particleA->enabled) continue;
		for (int j = i + 1; j < numParticles; j++)
		{
			Particle *particleB = &particles[j];
			vt_resolve_particle_collision(particleA, particleB);
		}
	}
}


void vt_enforce_colision_in_space_partition_cell(Particle particles[], SpacePartitionCell spacePartitionCell)
{
	for(int i = 0; i < spacePartitionCell.particleCount; i++)
	{
		Particle* particleA = &particles[spacePartitionCell.particles[i]];
		for(int j = i + 1; j < spacePartitionCell.particleCount; j++)
		{
			Particle* particleB = &particles[spacePartitionCell.particles[j]];
			vt_resolve_particle_collision(particleA, particleB);
		}
	}
}

void vt_resolve_particle_collision(Particle* particleA, Particle* particleB)
{
	static float relaxation_index = 0.1f;
	if(!particleB->enabled || !particleA->enabled) return;

	float radiusSum = particleA->radious + particleB->radious;

	ImVec2 axis = ig_vec2_diff(particleA->position, particleB->position);
	float dist = ig_vec2_length(axis);

	if (dist < radiusSum)
	{
		ImVec2 normal = ig_vec2_scale(axis, 1.0f / dist);
		float delta = (radiusSum - dist) * relaxation_index;

		if(dist < 0.001f)
		{
			dist = 0.001f;
			normal = (ImVec2){1.0, 0};
		}


		bool infA = (particleA->mass < 0);
		bool infB = (particleB->mass < 0);

		if (infA && infB)
		{
			return;
		}
		else if (infA)
		{
			particleB->position = ig_vec2_diff(particleB->position, ig_vec2_scale(normal, delta));
		}
		else if (infB)
		{
			particleA->position = ig_vec2_add(particleA->position, ig_vec2_scale(normal, delta));
		}
		else
		{
			float totalMass = particleA->mass + particleB->mass;
			float ratioA = particleB->mass / totalMass;
			float ratioB = particleA->mass / totalMass;

			particleA->position = ig_vec2_add(particleA->position, ig_vec2_scale(normal, delta * ratioA));
			particleB->position = ig_vec2_diff(particleB->position, ig_vec2_scale(normal, delta * ratioB));
		}
	}
}