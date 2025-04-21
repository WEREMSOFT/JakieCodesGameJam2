#include <GLFW/glfw3.h>
#include "image.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

ImageDef LoadTexture(const char *filename)
{
	ImageDef returnValue = {0};
	unsigned char *data = stbi_load(filename, &returnValue.width, &returnValue.height, &returnValue.channels, 4);
	if (!data)
		return returnValue;

	glGenTextures(1, &returnValue.textureId);
	glBindTexture(GL_TEXTURE_2D, returnValue.textureId);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, returnValue.width, returnValue.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	stbi_image_free(data);
	return returnValue;
}