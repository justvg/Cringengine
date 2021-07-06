#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#include <stdio.h>
int main()
{
	if (glfwInit())
	{
		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
		GLFWwindow* Window = glfwCreateWindow(1024, 720, "Cringengine", 0, 0);
		if (Window)
		{
			
		}
		else
		{
			printf("Can't create window\n");
		}
	}
	else
	{
		printf("Can't initalize GLFW\n");
	}

	return 0;
}