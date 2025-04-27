#ifndef __SOUND_H__
#define __SOUND_H__
#include <soloud_c.h>

typedef enum
{
	SPEECH_WELLCOME,
	SPEECH_YOU_WIN,
	SPEECH_GAME_OVER,
	SPEECH_COUNT
} GameSpeech;

typedef enum
{
	SFX_SELECT,
	SFX_SHOOT_HERO,
	SFX_ENEMY_ESCAPED,
	SFX_BLIP,
	SFX_JUMP,
	SFX_HURT,
	SFX_COIN,
	SFX_COUNT
} GameSFX;

typedef struct
{
	Soloud *soloud;
	Speech *speechs[SPEECH_COUNT];
	Sfxr *sfx[SFX_COUNT];
} Sound;

Sound soundCreate()
{
	Sound _this = {0};

	_this.soloud = Soloud_create();
	for (int i = 0; i < SPEECH_COUNT; i++)
	{
		_this.speechs[i] = Speech_create();
	}

	for (int i = 0; i < SFX_COUNT; i++)
	{
		_this.sfx[i] = Sfxr_create();
	}

	Speech_setText(_this.speechs[SPEECH_WELLCOME], "welcome!");
	Speech_setText(_this.speechs[SPEECH_YOU_WIN], "You Win!");
	Sfxr_loadPreset(_this.sfx[SFX_BLIP], SFXR_BLIP, 3247);
	Sfxr_loadPreset(_this.sfx[SFX_SELECT], SFXR_POWERUP, 3247);
	Sfxr_loadPreset(_this.sfx[SFX_SHOOT_HERO], SFXR_EXPLOSION, 3247);
	Sfxr_loadPreset(_this.sfx[SFX_ENEMY_ESCAPED], SFXR_HURT, 3247);
	Sfxr_loadPreset(_this.sfx[SFX_JUMP], SFXR_JUMP, 3247);
	Sfxr_loadPreset(_this.sfx[SFX_HURT], SFXR_HURT, 3247);
	Sfxr_loadPreset(_this.sfx[SFX_COIN], SFXR_COIN, 3247);

	Soloud_initEx(
		_this.soloud,
		SOLOUD_CLIP_ROUNDOFF | SOLOUD_ENABLE_VISUALIZATION,
		SOLOUD_AUTO, SOLOUD_AUTO, SOLOUD_AUTO, 2);

	Soloud_setGlobalVolume(_this.soloud, 4);

	return _this;
}

void soundPlaySfx(Sound _this, GameSFX id)
{
	Soloud_play(_this.soloud, _this.sfx[id]);
}

void sound_play_speech(Sound _this, GameSpeech id)
{
	Soloud_play(_this.soloud, _this.speechs[id]);
}

void soundDestroy(Sound _this)
{
	for (int i = 0; i < SPEECH_COUNT; i++)
	{
		Speech_destroy(_this.speechs[i]);
	}

	for (int i = 0; i < SFX_COUNT; i++)
	{
		Sfxr_destroy(_this.sfx[i]);
	}

	Soloud_deinit(_this.soloud);
	Soloud_destroy(_this.soloud);
}
#endif