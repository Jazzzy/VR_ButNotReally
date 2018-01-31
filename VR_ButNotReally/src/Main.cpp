#include "./render/RenderManager.h"
#include "./utils/Utils.h"

/* 

	Next Step for the renderer: Queue Families!

	Reference: https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families (in the Queue families section)

*/


int main() {
	
	/*
	
	Scope useful to be able to stop after every local main variable
	has been destroyed in case we want to check the state, print
	some messages or wait for character input to see the terminal output.

	*/

	{
		try {
			RenderManager render_manager;
			while (!render_manager.shouldClose()) {
				render_manager.update();
			}
		}
		catch (const std::exception& exception) {
			std::cerr << exception.what() << std::endl;
			std::cerr << "\nThe program will now be closed" << std::endl;
			pressToContinue();
			return EXIT_FAILURE;
		}
	}

	std::cout << "\nThe program will now be closed" << std::endl;
	pressToContinue();
	return EXIT_SUCCESS;

}

