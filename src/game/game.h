#ifndef __GAME_H__
#define __GAME_H__

#include <stdio.h>

typedef struct Game
{
	bool shouldRun;
	void (*init)(int width, int height, int windowScale, bool fullScreen);
	void (*updateWeb)(void);
	bool (*update)(void);
	void (*destroy)(void);
} Game;


typedef enum GameStateEnum
{
	GAME_STATE_READY,
	GAME_STATE_RUNNING,
	GAME_STATE_CREATING_PARTICLE,
	GAME_STATE_CREATING_FIXED_SIZE_GEAR,
	GAME_STATE_CREATING_GEAR,
	GAME_STATE_CREATING_FIXED_PARTICLE,
	GAME_STATE_CREATING_STANDARD_PARTICLE,
	GAME_STATE_CREATING_NEAR_CONSTRAINT,
	GAME_STATE_CREATING_FAR_CONSTRAINT,
	GAME_STATE_CREATING_LINK_CONSTRAINT,
	GAME_STATE_WON,
	GAME_STATE_COUNT
} GameStateEnum;

typedef enum GenericSubstateEnum
{
	SUBSTATE_READY,
	SUBSTATE_DRAWING
} GenericSubstateEnum;

Game create_game();

#endif