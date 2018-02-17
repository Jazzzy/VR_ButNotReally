#include "./render/Renderer.h"
#include "./utils/Utils.h"

int main() {

	/*

	Scope useful to be able to stop after every local main variable
	has been destroyed in case we want to check the state, print
	some messages or wait for character input to see the terminal output.

	*/

	{
		try {
			Renderer renderer;
			while (!renderer.shouldClose()) {
				glfwPollEvents();
				renderer.beginFrame();
				renderer.endFrame();
			}
		}
		catch (const std::exception& exception) {
			std::cerr << exception.what() << std::endl;
			std::cerr << "\nThe program will now be closed since we cannot execute further" << std::endl;
			pressToContinue();
			return EXIT_FAILURE;
		}
	}

	std::cout << "\nThe program will now be closed" << std::endl;
	pressToContinue();
	return EXIT_SUCCESS;
}

