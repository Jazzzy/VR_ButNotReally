#include "RenderManager.h"
#include "../Configuration.h"
#include <gsl/gsl>
#include <map>
#include <algorithm>

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
	pickPhysicalDevice();
}

auto RenderManager::loopIteration() noexcept -> void {
	glfwPollEvents();
}

auto RenderManager::cleanup() noexcept -> void {
	destroyDebugReportCallbackEXT(m_instance, m_debug_callback, nullptr);
	vkDestroyInstance(m_instance, nullptr);
	m_window.reset();
	glfwTerminate();
}

auto RenderManager::createInstance() noexcept(false) -> VkInstance {

	if (config::validation_layers_enabled && !checkValidationLayerSupport()) {
		throw std::runtime_error("Validation layers were requested but were not available.");
	}

	/* - Setting up Application Info - */
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

	/* - Setting up Vulkan Validation Layers - */
	[[gsl::suppress(type.1)]]
	if (config::validation_layers_enabled) {
		create_info.enabledLayerCount = gsl::narrow<uint>(config::validation_layers.size());
		create_info.ppEnabledLayerNames = config::validation_layers.data();
	}
	else {
		std::cout << "Validation layers are disabled as this is a release build" << std::endl << std::endl;
		create_info.enabledLayerCount = 0;
	}


	/* - Setting up Vulkan Extensions - */
	auto extensions_required = getRequiredExtensions();

	/*
	We check the extensions available in the current instance.
	*/
	auto extensions_available = std::vector<VkExtensionProperties>();
	{
		auto extension_count = uint{ 0 };
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);
		extensions_available.resize(extension_count);
		vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, extensions_available.data());

		printInstanceExtensions(extensions_available);
	}

	if (checkInstanceExtensionsNamesAvailable(extensions_required, extensions_available)) {
		create_info.enabledExtensionCount = gsl::narrow<uint>(extensions_required.size());
		create_info.ppEnabledExtensionNames = extensions_required.data();
	}
	else {
		throw std::runtime_error("The required vulkan extensions were not available in the system");
	}

	VkInstance instance{};
	const auto result = vkCreateInstance(&create_info, nullptr, &instance);
	if (result != VK_SUCCESS) {
		throw std::runtime_error("We could not create the vulkan instance");
	}


	return instance;
}

[[gsl::suppress(bounds)]] auto RenderManager::printInstanceExtensions(const std::vector<VkExtensionProperties>& extensions) const -> void {

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

auto RenderManager::checkInstanceExtensionsNamesAvailable(const std::vector<const char*>& required_extensions, const std::vector<VkExtensionProperties>& available_extensions) const -> bool {

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
				const auto aux = std::string_view(e.extensionName);
				found = aux.compare(required_extension);
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
			const auto layer_present_view = std::string_view(layer_present.layerName);
			if (layer_present_view.compare(layer_necessary) == 0) {
				found_layer = true;
				break;
			}
		}
		if (found_layer) {
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

[[gsl::suppress(bounds.1)]] auto RenderManager::getRequiredExtensions() const noexcept -> std::vector<const char*> {

	auto extension_count = uint{ 0 };
	const char ** glfw_extensions = glfwGetRequiredInstanceExtensions(&extension_count);


	auto extensions = std::vector<const char*>(glfw_extensions, glfw_extensions + extension_count);


	if (config::extensions_enabled) {
		for (auto extension_to_add : config::extensions) {
			extensions.push_back(extension_to_add);
		}
	}


	return extensions;
}

#pragma warning( push )
#pragma warning( disable : 4229)
auto VKAPI_ATTR VKAPI_CALL RenderManager::debugReportCallback(
	VkDebugReportFlagsEXT                       flags,
	VkDebugReportObjectTypeEXT                  object_type,
	uint64_t                                    object,
	size_t                                      location,
	int32_t                                     msg_code,
	const char*                                 layer_prefix,
	const char*                                 msg,
	void*                                       user_data
) -> VkBool32 {

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

	[[gsl::suppress(6285)]]{
	if (!config::extensions_enabled || !config::validation_layers_enabled) {
		return;
	}
	}

	VkDebugReportCallbackCreateInfoEXT create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
	create_info.flags =
		(VK_DEBUG_REPORT_DEBUG_BIT_EXT
			| VK_DEBUG_REPORT_ERROR_BIT_EXT
			| VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT
			| VK_DEBUG_REPORT_WARNING_BIT_EXT
			| VK_DEBUG_REPORT_INFORMATION_BIT_EXT);

	[[gsl::suppress(type.1)]]{
	create_info.pfnCallback = reinterpret_cast<PFN_vkDebugReportCallbackEXT>(debugReportCallback);
	}

		//createDebugReportCallbackEXT USE

		if (createDebugReportCallbackEXT(m_instance, &create_info, nullptr, &m_debug_callback) != VK_SUCCESS) {
			throw std::runtime_error("We could't setup the debug callback function");
		}
}

auto RenderManager::createDebugReportCallbackEXT(
	const VkInstance& instance,
	const VkDebugReportCallbackCreateInfoEXT * create_info,
	const VkAllocationCallbacks* allocator,
	VkDebugReportCallbackEXT* callback
) noexcept -> VkResult {

	[[gsl::suppress(type.1)]]{
	auto function = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugReportCallbackEXT"));

	if (function) {
		return function(instance, create_info, allocator, callback);
	}
	else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
	}

}

auto RenderManager::destroyDebugReportCallbackEXT(
	const VkInstance& instance,
	const VkDebugReportCallbackEXT& callback,
	const VkAllocationCallbacks* allocator
) noexcept -> void {
	[[gsl::suppress(type.1)]]{
	auto function = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugReportCallbackEXT"));

	if (function) {
		function(instance, callback, allocator);
	}
	}
}

auto RenderManager::pickPhysicalDevice() -> void {

	auto count = uint{};

	if (m_instance == VkInstance{}) {
		throw std::runtime_error("The vulkan instance is not available so we cannot pick a physical device");
	}

	vkEnumeratePhysicalDevices(m_instance, &count, nullptr);

	if (count == 0) {
		throw std::runtime_error("We cannot continue because there is no physical devices (GPUs) that support vulkan");
	}

	auto devices = std::vector<VkPhysicalDevice>(count);
	vkEnumeratePhysicalDevices(m_instance, &count, devices.data());


	auto devices_sorted = std::multimap<int, VkPhysicalDevice>{};

	for (const auto& device : devices) {
		auto[valid, score] = physicalDeviceSuitability(device);
		if (valid) {
			devices_sorted.insert(std::make_pair(score, device));
		}
	}

	if (devices_sorted.rbegin()->first > 0) {
		m_physical_device = devices_sorted.rbegin()->second;
	}
	else {
		throw std::runtime_error("We cannot continue because there is no suitable physical device (GPU)");
	}

	std::cout << "The available physical devices are the following" << std::endl;
	for (const auto& device : devices_sorted) {
		std::cout << m_physical_device << std::endl;
	}
	std::cout << std::endl;

	std::cout << "The selected physical device is:" << std::endl << m_physical_device << std::endl;

}

auto RenderManager::physicalDeviceSuitability(const VkPhysicalDevice & device) const noexcept -> std::tuple<bool, int> {

	auto features = VkPhysicalDeviceFeatures{};
	auto properties = VkPhysicalDeviceProperties{};
	vkGetPhysicalDeviceProperties(device, &properties);
	vkGetPhysicalDeviceFeatures(device, &features);

	auto score = 0;

	/* - Checking if something makes us mark the device as not suitable - */
	if (!features.geometryShader) {
		return std::make_tuple(false, 0);
	}

	/* - Adding points to the score - */
	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += config::gpu::discrete_gpu_bonus;
	}
	else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
		score += config::gpu::integrated_gpu_bonus;
	}

	score += properties.limits.maxImageDimension2D;

	return std::make_tuple(true, score);
}









/* --- Non members of the RenderManager Class --- */

auto operator<<(std::ostream & stream, const VkPhysicalDevice& device)->std::ostream& {
	auto features = VkPhysicalDeviceFeatures{};
	auto properties = VkPhysicalDeviceProperties{};
	vkGetPhysicalDeviceProperties(device, &properties);
	vkGetPhysicalDeviceFeatures(device, &features);

	[[gsl::suppress(bounds.3)]]{
	auto upper_name = std::string(properties.deviceName);

	std::transform(upper_name.begin(), upper_name.end(), upper_name.begin(), toupper);

	stream << "[" << upper_name << "]" << std::endl;

	stream << " - Device ID: " << properties.deviceID << std::endl;
	stream << " - Vendor ID: " << properties.vendorID << std::endl;
	stream << " - API Version: " << properties.apiVersion << std::endl;
	stream << " - Device Name: " << properties.deviceName << std::endl;
	
	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		stream << " - Device Type : " << "Discrete GPU" << std::endl;
	}
	else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) {
		stream << " - Device Type : " << "Integrated GPU" << std::endl;
	}
	else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU) {
		stream << " - Device Type : " << "CPU" << std::endl;
	}
	else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU) {
		stream << " - Device Type : " << "Virtual GPU" << std::endl;
	}
	else if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_OTHER) {
		stream << " - Device Type : " << "Other" << std::endl;
	}
	stream << " - Driver Version ID: " << properties.driverVersion << std::endl;
	}

	return stream;
}


/*Functors*/

auto GLFWWindowDestroyer::operator()(GLFWwindow* ptr) noexcept -> void {
	glfwDestroyWindow(ptr);
}
