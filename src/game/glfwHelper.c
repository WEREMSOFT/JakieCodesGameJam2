#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdbool.h>

GLFWwindow *glfwCreateAndInitWindow(int width, int height, int windowScale, bool fullScreen)
{
	GLFWwindow* window;
	glfwInit();
	GLFWmonitor *monitor = glfwGetPrimaryMonitor();
	const GLFWvidmode *mode = glfwGetVideoMode(monitor);

	glfwWindowHint(GLFW_RED_BITS, mode->redBits);
	glfwWindowHint(GLFW_GREEN_BITS, mode->greenBits);
	glfwWindowHint(GLFW_BLUE_BITS, mode->blueBits);
	glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

	int screenWidth = mode->width;
	int screenHeight = mode->height;

	if (!fullScreen)
	{
		screenWidth = width * windowScale;
		screenHeight = height * windowScale;
		monitor = NULL;
	}

	window = glfwCreateWindow(screenWidth, screenHeight, "Frame Buffer", monitor, NULL);

	glfwMakeContextCurrent(window);
	gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);

	glCreateShader(GL_VERTEX_SHADER);

	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
	glfwSetCursorPos(window, 0, 0);
	return window;
}