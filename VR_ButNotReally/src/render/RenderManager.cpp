#include "RenderManager.h"
#include "../Configuration.h"
#include "gsl.h"

RenderManager::RenderManager() : m_instance() {
	initWindow();
	initVulkan();
};

RenderManager::~RenderManager() {
	cleanup();
};

auto RenderManager::shouldClose() const noexcept -> bool {
	return glfwWindowShouldClose(m_window.get());
}

auto RenderManager::update() noexcept -> void {
	loopIteration();
}

auto RenderManager::initWindow() noexcept -> void {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_window = WindowPtr(glfwCreateWindow(
		config::initial_window_width,
		config::initial_window_heigth,
		config::app_name, nullptr, nullptr));


	 
}

auto RenderManager::initVulkan() noexcept(false) -> void {
	m_instance = createInstance();
	setupDebugCallback();
}

auto RenderManager::loopIteration() noexcept -> void {
	glfwPollEvents();
}

auto RenderManager::cleanup() noexcept -> void {
	destroyDebugReportCallbackEXT(m_instance, m_debug_callback, nullptr);
	vkDestroyInstance(m_instance, nullptr);
	glfwTerminate();
}

auto RenderManager::createInstance() noexcept(false) -> VkInstance {


	if (config::validation_layers_enabled && !checkValidationLayerSupport()) {
		throw std::runtime_error("Validation layers were requested but were not available.");
	}


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

	[[gsl::suppress(type.1)]]
	if (config::validation_layers_enabled) {
		create_info.enabledLayerCount = gsl::narrow<uint>(config::validation_layers.size());
		create_info.ppEnabledLayerNames = config::validation_layers.data();
	} else {
		create_info.enabledLayerCount = gsl::narrow<uint>(config::validation_layers.size());
	}

	auto extensions_required = getRequiredExtensions();
	create_info.enabledExtensionCount = gsl::narrow<uint>(extensions_required.size());
	create_info.ppEnabledExtensionNames = extensions_required.data();

	create_info.enabledLayerCount = 0;

	VkInstance instance{};
	const auto result = vkCreateInstance(&create_info, nullptr, &instance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("We could not create the vulkan instance");
	}

	/*
	We check the extensions available in the current instance.
	*/
	auto extensions_available = std::vector<VkExtensionProperties>();
	{
		auto extension_count = uint{ 0 };
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
		extensions_available.resize(extension_count);
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions_available.data());
	}

	printInstanceExtensions(extensions_available);

	checkInstanceExtensionsNamesAvailable(extensions_required, extensions_available);

	return instance;
}

[[gsl::suppress(bounds)]] auto RenderManager::printInstanceExtensions(const std::vector<VkExtensionProperties> extensions) const -> void {

	std::cout << "Printing available extensions in the current vulkan instance" << std::endl;

	if (extensions.size() == 0) {
		std::cout << "\tNo available extensions in the current vulkan instance" << std::endl;
		return;
	}

	for (const auto& e : extensions) {
		std::cout << "\t[" << e.extensionName << "]" << std::endl;
	}

	std::cout << std::endl;

}

auto RenderManager::checkInstanceExtensionsNamesAvailable(const std::vector<const char*> required_extensions, const std::vector<VkExtensionProperties> available_extensions) const -> bool {

	if (required_extensions.size() <= 0) {
		std::cerr << "There are no exceptions required, and there should be some if we want to check them" << std::endl;
		return false;
	}

	std::cout << "Checking that all the necessary vulkan extensions are available" << std::endl;
	auto all_found = true;
	for (auto required_extension : required_extensions) {
		auto found = false;
		[[gsl::suppress(bounds)]]{
		std::cout << "\t[" << required_extension << "] is required";
		}
		for (const auto& e : available_extensions) {
			found = strcmp(e.extensionName, required_extension);
			if (found) break;
		}
		if (found) {
			std::cout << " and available" << std::endl;
			continue;
		}
		else {
			std::cout << " and NOT available" << std::endl;
			all_found = false;
		}
	}

	std::cout << std::endl;
	return all_found;
}

auto RenderManager::checkValidationLayerSupport() const noexcept -> bool {

	uint layers_found;

	vkEnumerateInstanceLayerProperties(&layers_found, nullptr);

	auto layer_properties = std::vector<VkLayerProperties>(layers_found);
	vkEnumerateInstanceLayerProperties(&layers_found, layer_properties.data());

	std::cout << "Checking that all the necessary vulkan layers are available" << std::endl;
	auto found_all_layers = true;
	for (const auto& layer_necessary : config::validation_layers) {
		auto found_layer = false;
		std::cout << "\t[" << layer_necessary << "] is required";
		for (const auto& layer_present : layer_properties) {
			if (strcmp(layer_present.layerName, layer_necessary) == 0) {
				found_layer = true;
				break;
			}
		}
		if(found_layer){
			std::cout << " and available" << std::endl;
			continue;
		}
		else {
			std::cout << " and NOT available" << std::endl;
			found_all_layers = false;
		}
	}
	
	std::cout << std::endl;
	return found_all_layers;
}

auto RenderManager::getRequiredExtensions() const noexcept -> std::vector<const char*> {

	auto extension_count = uint{0};
	const char ** glfw_extensions = glfwGetRequiredInstanceExtensions(&extension_count);

	[[gsl::suppress(bounds.1)]]auto extensions = std::vector<const char*>(glfw_extensions, glfw_extensions + extension_count);

	if (config::extensions_enabled) {
		for (auto extension_to_add : config::extensions) {
			extensions.push_back(extension_to_add);
		}
	}

	return extensions;
}

#pragma warning( push )
#pragma warning( disable : 4229)
auto RenderManager::debugReportCallback(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  object_type,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     msg_code,
	const char*                                 layer_prefix,
	const char*                                 msg,
	void*                                       user_data
) -> VKAPI_ATTR VkBool32 VKAPI_CALL {

	std::cerr << "Validation Layer Message [" << layer_prefix << "]: ";

	if ((flags & VK_DEBUG_REPORT_DEBUG_BIT_EXT) != 0) {
		std::cerr << "[DEBUG: " << msg_code << "]: " << msg << std::endl;
	}
	else if ((flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) != 0) {
		std::cerr << "[ERROR: " << msg_code << "]: " << msg << std::endl;
	}
	else if ((flags & VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) != 0) {
		std::cerr << "[PERFORMANCE WARNING: " << msg_code << "]: " << msg << std::endl;
	}
	else if ((flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) != 0) {
		std::cerr << "[WARNING: " << msg_code << "]: " << msg << std::endl;
	}
	else if ((flags & VK_DEBUG_REPORT_INFORMATION_BIT_EXT) != 0) {
		std::cerr << "[INFO: " << msg_code << "]: " << msg << std::endl;
	}
	else {
		std::cerr << "[UNKNOWN: " << msg_code << "]: The type of error is not known, the message states: " << msg << std::endl;
	}

	std::cerr << std::endl;

	return VK_FALSE;
}
#pragma warning( pop )

auto RenderManager::setupDebugCallback() -> void {
	
	if (!config::extensions_enabled || !config::validation_layers_enabled) {
		return;
	}

	VkDebugReportCallbackCreateInfoEXT create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	create_info.flags =
		( VK_DEBUG_REPORT_DEBUG_BIT_EXT
		| VK_DEBUG_REPORT_ERROR_BIT_EXT 
		| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
		| VK_DEBUG_REPORT_WARNING_BIT_EXT
		| VK_DEBUG_REPORT_INFORMATION_BIT_EXT);
	create_info.pfnCallback = reinterpret_cast<PFN_vkDebugReportCallbackEXT>(debugReportCallback);

	//createDebugReportCallbackEXT USE

	if (createDebugReportCallbackEXT(m_instance, &create_info, nullptr, &m_debug_callback) != VK_SUCCESS) {
		throw std::runtime_error("We could't setup the debug callback function");
	}
} 

auto RenderManager::createDebugReportCallbackEXT(
	VkInstance instance,
	const VkDebugReportCallbackCreateInfoEXT * create_info,
	const VkAllocationCallbacks* allocator,
	VkDebugReportCallbackEXT* callback
) -> VkResult {
	 
	auto function = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT"));

	if (function) {
		return function(instance, create_info, allocator, callback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}

}

auto RenderManager::destroyDebugReportCallbackEXT(
	VkInstance instance,
	VkDebugReportCallbackEXT callback,
	const VkAllocationCallbacks* allocator
)-> void {
	auto function = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT"));

	if (function) {
		function(instance, callback, allocator);
	}
}


/* --- Non members of the RenderManager Class --- */


/*Functors*/

auto GLFWWindowDestroyer::operator()(GLFWwindow* ptr) noexcept -> void {
	glfwDestroyWindow(ptr);
}
