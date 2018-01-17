#include "RenderManager.h"
#include "../Configuration.h"


RenderManager::RenderManager() : m_instance() {
	initWindow();
	initVulkan();
};


RenderManager::~RenderManager() {
	cleanup();
};

auto RenderManager::shouldClose() const noexcept -> bool{
	return glfwWindowShouldClose(m_window.get());
}

auto RenderManager::update() noexcept -> void {
	loopIteration();
}


auto RenderManager::initWindow() noexcept -> void  {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_window = WindowPtr(glfwCreateWindow(
		config::initial_window_width,
		config::initial_window_heigth,
		config::app_name, nullptr, nullptr));



}

auto RenderManager::initVulkan() noexcept(false) -> void {
	createInstance();
}

auto RenderManager::loopIteration() noexcept -> void {
	glfwPollEvents();
}

auto RenderManager::cleanup() noexcept -> void {
	vkDestroyInstance(m_instance, nullptr);
	glfwTerminate();
}


auto RenderManager::createInstance() noexcept(false) -> void {

	auto app_info = VkApplicationInfo{};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = config::app_name;
	app_info.applicationVersion = VK_MAKE_VERSION(config::major_version, config::minor_version, config::patch_version);
	app_info.pEngineName = config::engine_name;
	app_info.applicationVersion = VK_MAKE_VERSION(config::major_version, config::minor_version, config::patch_version);
	app_info.apiVersion = VK_API_VERSION_1_0;
	app_info.pNext = nullptr;

	auto create_info = VkInstanceCreateInfo{};
	create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	create_info.pApplicationInfo = &app_info;
	
	const char** glfw_extensions = nullptr;
	auto glfw_extension_count = uint{ 0 };

	{
		glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

		create_info.enabledExtensionCount = glfw_extension_count;
		create_info.ppEnabledExtensionNames = glfw_extensions;
	}

	create_info.enabledLayerCount = 0;

	const auto result = vkCreateInstance(&create_info, nullptr, &m_instance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("We could not create the vulkan instance");
	}

	/*
	We check the extensions available in the current instance.
	*/
	auto extension_vector = std::vector<VkExtensionProperties>();
	{
		auto extension_count = uint{ 0 };
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
		extension_vector.resize(extension_count);
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extension_vector.data());
	}

	printInstanceExtensions(extension_vector);

	checkInstanceExtensionsNamesAvailable(glfw_extensions, glfw_extension_count, extension_vector);

}

[[gsl::suppress(bounds)]] auto RenderManager::printInstanceExtensions(const std::vector<VkExtensionProperties> extensions) const -> void {

	std::cout << "Printing available extensions in the current vulkan instance" << std::endl;

	if (extensions.size() == 0) {
		std::cout << "\tNo available extensions in the current vulkan instance" << std::endl;
		return;
	}

	for(const auto& e : extensions) {
		std::cout << "\t[" << e.extensionName <<  "]" << std::endl;
	}

	std::cout << std::endl;

}

auto RenderManager::checkInstanceExtensionsNamesAvailable(const char** const required_names, const uint name_count, const std::vector<VkExtensionProperties> available_extensions) const -> bool {

	if (name_count <= 0) {
		std::cerr << "There are no exceptions required, and there should be some if we want to check them" << std::endl;
		return false;
	}

	if (required_names == nullptr) {
		std::cerr << "The names of the extensions required cannot be accessed" << std::endl;
		return false;
	}

	std::cout << "Checking that all the necessary vulkan extensions are available" << std::endl;
	auto all_found = true;
	for (auto i = 0u; i < name_count; ++i) {
		auto found = false;
		for (const auto& e : available_extensions) {
			[[gsl::suppress(bounds)]]{
			std::cout << "\t[" << required_names[i] << "] is required";
			found = strcmp(e.extensionName, required_names[i]);
			if (found) break;
			}
		}
		if (found) {
			std::cout << " and available" << std::endl;
			continue;
		}
		else {
			std::cout << " and NOT available";
		}
		all_found = false;
	}

	return all_found;

}




/*Functors*/

auto GLFWWindowDestroyer::operator()(GLFWwindow* ptr) -> void {
	glfwDestroyWindow(ptr);
}
