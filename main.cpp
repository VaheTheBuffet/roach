#include <iostream>
#include <vulkan/vulkan.h>
#include <GLFW/glfw3.h>

#define WIDTH 800
#define HEIGHT 600

static VkInstance instance;
static GLFWwindow *window;

int main()
{
	//create window
	if(!glfwInit()) {
		throw std::runtime_error("window creation failed");
	}
	//glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
	
	window = glfwCreateWindow(WIDTH, HEIGHT, "Roach", NULL, NULL);
	
	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Roach";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "No Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_0;
	
	VkInstanceCreateInfo instance_create_info = {};
	instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_create_info.pApplicationInfo = &app_info;
	vkCreateInstance(&instance_create_info, NULL, &instance);

	glfwMakeContextCurrent(window);
	while(!glfwWindowShouldClose(window)) {
		glfwPollEvents();
		glfwSwapBuffers(window);
	}

	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
