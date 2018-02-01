#include <limits>
#include "RenderManager.h"
#include "../Configuration.h"
#include <map>
#include <set>
#include <algorithm>
#include <vulkan/vk_platform.h>
#undef max
#undef min

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
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
}

auto RenderManager::loopIteration() noexcept -> void {
	glfwPollEvents();
}

auto RenderManager::cleanup() noexcept -> void {
	vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
	vkDestroyDevice(m_device, nullptr);
	destroyDebugReportCallbackEXT(m_instance, m_debug_callback, nullptr);
	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
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


	if (config::instance_extensions_enabled) {
		for (auto extension_to_add : config::instance_extensions) {
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
	if (!config::instance_extensions_enabled || !config::validation_layers_enabled) {
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

auto RenderManager::createSurface() -> void {

#ifdef _WIN32
	auto create_info = VkWin32SurfaceCreateInfoKHR{};
	create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	create_info.hwnd = glfwGetWin32Window(m_window.get());
	create_info.hinstance = GetModuleHandle(nullptr);

	[[gsl::suppress(type.1)]]{
	auto createWin32SurfaceKHR = reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(vkGetInstanceProcAddr(m_instance,
		"vkCreateWin32SurfaceKHR"));

	if (!createWin32SurfaceKHR || createWin32SurfaceKHR(m_instance, &create_info, nullptr, &m_surface) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a window surface");
	}
	}
#else
	if (glfwCreateWindowSurface(m_instance, m_window.get(), nullptr, &m_surface) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a Win32 surface");
	}
#endif

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

	std::cout << "The available physical devices are the following" << std::endl;
	for (const auto& device : devices) {
		std::cout << device << std::endl;
	}
	std::cout << std::endl;

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

	std::cout << "The selected physical device is:" << std::endl << m_physical_device << std::endl;

}

auto RenderManager::physicalDeviceSuitability(const VkPhysicalDevice & device) const noexcept -> std::tuple<bool, int> {

	auto features = VkPhysicalDeviceFeatures{};
	auto properties = VkPhysicalDeviceProperties{};
	vkGetPhysicalDeviceProperties(device, &properties);
	vkGetPhysicalDeviceFeatures(device, &features);

	auto score = 0;

	auto family_indices = findQueueFamilies(device, PrintOptions::full);

	const auto device_extensions_supported = checkDeviceExtensionSupport(device);

	auto swap_chain_suitable = false;
	if (device_extensions_supported) {
		auto swap_chain_support = querySwapChainSupport(device);
		swap_chain_suitable = !swap_chain_support.formats.empty() &&
			!swap_chain_support.present_modes.empty();
	}

	/* - Checking if something makes us mark the device as not suitable - */
	if (!features.geometryShader || !family_indices.isComplete() || !device_extensions_supported || !swap_chain_suitable) {
		return std::make_tuple(false, 0);
	}

	if (family_indices.graphics_family == family_indices.present_family) {
		score += config::gpu::same_queue_family;
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

auto RenderManager::findQueueFamilies(const VkPhysicalDevice& physical_device, PrintOptions print_options) const -> QueueFamilyIndices {

	auto queue_family_count = uint{};
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);

	auto queue_families = std::vector<VkQueueFamilyProperties>(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families.data());

	auto indices = QueueFamilyIndices{};

	if (print_options == PrintOptions::full)
		std::cout << "The queue families for the physical device [" << getPhysicalDeviceName(physical_device) << "] are the following" << std::endl;

	{
		auto i = 0;
		for (const VkQueueFamilyProperties& family : queue_families) {

			if (print_options == PrintOptions::full)
				std::cout << " - " << getVulkanQueueFlagNames(family.queueFlags) << std::endl;

			if (family.queueCount > 0) {
				if (family.queueFlags & config::gpu::required_family_flags) {
					indices.graphics_family = i;
				}
				auto present_support = VkBool32{};
				vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, m_surface, &present_support);
				if (present_support) {
					indices.present_family = i;
				}
			}

			if (indices.isComplete()) break;

			i++;
		}
	}

	if (print_options == PrintOptions::full)
		std::cout << std::endl;

	return indices;
}

auto RenderManager::createLogicalDevice() -> void {

	m_queue_family_indices = findQueueFamilies(m_physical_device, PrintOptions::none);

	auto queue_create_infos = std::vector<VkDeviceQueueCreateInfo>{};
	auto unique_queue_families = std::set<int>{ m_queue_family_indices.graphics_family,m_queue_family_indices.present_family };

	const auto queue_priority = 1.0f;

	for (auto queue_familie : unique_queue_families) {
		auto queue_create_info = VkDeviceQueueCreateInfo{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_familie;
		queue_create_info.queueCount = config::gpu::queue_amount;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}


	// Complete here the features that we need from the physical devices
	const auto physical_device_features = VkPhysicalDeviceFeatures{};

	auto create_info = VkDeviceCreateInfo{};
	create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	create_info.pQueueCreateInfos = queue_create_infos.data();
	create_info.queueCreateInfoCount = gsl::narrow<uint>(queue_create_infos.size());
	create_info.pEnabledFeatures = &physical_device_features;

	create_info.enabledExtensionCount = gsl::narrow<uint>(config::device_extensions.size());
	create_info.ppEnabledExtensionNames = config::device_extensions.data();

	if (config::validation_layers_enabled) {
		create_info.enabledLayerCount = gsl::narrow<uint>(config::validation_layers.size());
		create_info.ppEnabledLayerNames = config::validation_layers.data();
	}
	else {
		create_info.enabledLayerCount = 0;
	}

	if (vkCreateDevice(m_physical_device, &create_info, nullptr, &m_device) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a logical device");
	}

	vkGetDeviceQueue(m_device, m_queue_family_indices.graphics_family, 0, &m_graphics_queue);

	vkGetDeviceQueue(m_device, m_queue_family_indices.present_family, 0, &m_present_queue);

}

auto RenderManager::checkDeviceExtensionSupport(const VkPhysicalDevice& device) const -> bool {

	auto extension_count = uint{};
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

	auto available_extensions = std::vector<VkExtensionProperties>(extension_count);
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.data());

	for (const auto& required_extension : config::device_extensions) {
		bool found = false;
		for (const auto& available_extension : available_extensions) {
			const auto aux = std::string_view(available_extension.extensionName);
			if (aux.compare(required_extension)) {
				found = true;
				break;
			}
		}
		if (found == false) {
			return false;
		}
	}
	return true;
}

auto RenderManager::querySwapChainSupport(const VkPhysicalDevice& device) const->SwapChainSupportDetails {

	auto details = SwapChainSupportDetails{};

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

	auto format_count = uint{};

	vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);

	if (format_count > 0) {
		details.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.data());
	}


	auto mode_count = uint{};

	vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &mode_count, nullptr);

	if (mode_count > 0) {
		details.present_modes.resize(mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &mode_count, details.present_modes.data());
	}

	return details;
}


auto RenderManager::pickSurfaceChainFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const ->VkSurfaceFormatKHR {


	if (available_formats.size() == 1 && available_formats[0].format == VK_FORMAT_UNDEFINED) {	// Vulkan doesn't have a preferred format
		return VkSurfaceFormatKHR{ VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR };
	}

	for (const auto& format : available_formats) {
		if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			return format;
	}

	/*

	We could very well try to sort all the available formats by how fit they are
	for the renderer. That said, in the majority of cases it is ok to pick the first
	format specified.

	*/

	return available_formats[0];

}

auto RenderManager::pickSurfacePresentMode(const std::vector<VkPresentModeKHR>& available_modes)  const noexcept -> VkPresentModeKHR {


	for (const auto& desired_mode : config::preferred_present_modes_sorted) {
		for (const auto& available_mode : available_modes) {
			if (desired_mode == available_mode) return desired_mode;
		}
	}


	return VK_PRESENT_MODE_FIFO_KHR;	// We are guaranteed that this mode is supported, so in theory we should never reach this point.
}

auto RenderManager::pickSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)  const noexcept -> VkExtent2D {

	if (capabilities.currentExtent.width != std::numeric_limits<uint>::max()) {
		return capabilities.currentExtent;
	}
	else {
		auto extent = VkExtent2D{
			config::initial_window_width,
			config::initial_window_heigth
		};

		extent.width = std::max(capabilities.minImageExtent.width,
			std::min(capabilities.maxImageExtent.width,
				extent.width));

		extent.height = std::max(capabilities.minImageExtent.height,
			std::min(capabilities.maxImageExtent.height,
				extent.height));

		return extent;
	}

}


auto RenderManager::createSwapChain() -> void {

	auto swap_chain_support = querySwapChainSupport(m_physical_device);

	const auto surface_format = pickSurfaceChainFormat(swap_chain_support.formats);
	const auto present_mode = pickSurfacePresentMode(swap_chain_support.present_modes);
	const auto extent = pickSwapExtent(swap_chain_support.capabilities);

	std::cout << "Selected present mode: " << present_mode << std::endl << std::endl;

	/*

	We now decide the amount of images in the queue, since we want to implement triple
	buffering we will add one to the minimum amount required by their implementation.

	*/
	auto image_count = uint{};
	[[gsl::suppress(type.4)]]{
	image_count = uint{ swap_chain_support.capabilities.minImageCount + 1u };
	}

		if (swap_chain_support.capabilities.maxImageCount > 0 &&
			image_count > swap_chain_support.capabilities.maxImageCount) {	// If our image count is bigger than the maximum available
			image_count = swap_chain_support.capabilities.maxImageCount;
		}

	std::cout << "Number of images required in Swap Chain: " << image_count << std::endl << std::endl;

	auto create_info = VkSwapchainCreateInfoKHR{};
	create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	create_info.surface = m_surface;
	create_info.minImageCount = image_count;
	create_info.imageFormat = surface_format.format;
	create_info.imageColorSpace = surface_format.colorSpace;
	create_info.imageExtent = extent;
	create_info.preTransform = swap_chain_support.capabilities.currentTransform;

	/*
	This the amount of layers for each images, it only is more
	than 1 when doing stereoscopic 3D or something like that.
	*/
	create_info.imageArrayLayers = 1;
	/*
	This means that we will render directly to the image. In case we
	want to render to other image and then swap the memory to this one
	(for postprocessing effects) we could use VK_IMAGE_USAGE_TRANSFER_DST_BIT
	*/
	create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;


	auto queue_family_indices = std::vector<uint>{
		gsl::narrow<uint>(m_queue_family_indices.graphics_family) ,
		gsl::narrow<uint>(m_queue_family_indices.present_family)
	};

	if (m_queue_family_indices.graphics_family != m_queue_family_indices.present_family) {
		create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
		create_info.queueFamilyIndexCount = 2;
		create_info.pQueueFamilyIndices = queue_family_indices.data();
	}
	else {
		create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		create_info.queueFamilyIndexCount = 0;
		create_info.pQueueFamilyIndices = nullptr;
	}
	/*
	This is used to especify if we should use the alpha channel to blend with
	other windows in the windowing system.
	*/
	create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	create_info.presentMode = present_mode;
	/*
	This means that we don't care about pixels that are not visible,
	for example, if they are hidden by another window.
	*/
	create_info.clipped = VK_TRUE;

	/*
	When we are replacing an old swap chain that is no longer viable
	or optimal we have to reference the old one in the following variable.
	*/
	create_info.oldSwapchain = VK_NULL_HANDLE;

	if (vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swap_chain) != VK_SUCCESS) {
		throw std::runtime_error("We could not create the swapchain");
	}

	vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, nullptr);
	m_swap_chain_images.resize(image_count);
	vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, m_swap_chain_images.data());

	std::cout << "Number of images acquiesced in Swap Chain: " << image_count << std::endl << std::endl;

	/*
	We store these two for future use
	*/
	m_swap_chain_image_format = surface_format.format;
	m_swap_chain_extent = extent;
}


