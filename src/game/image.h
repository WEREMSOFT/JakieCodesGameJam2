#ifndef __IMAGE_H__
#define __IMAGE_H__

typedef struct ImageDef
{
	int textureId;
	int width, height, channels;
} ImageDef;

static ImageDef image = {0};

ImageDef LoadTexture(const char *filename);

#endif