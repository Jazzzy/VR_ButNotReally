#include <limits>
#include "RenderManager.h"
#include "../Configuration.h"
#include <map>
#include <set>
#include <algorithm>
#include <vulkan/vk_platform.h>
#include "RenderData.h"
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

	m_window = WindowPtr(glfwCreateWindow(
		config::initial_window_width,
		config::initial_window_heigth,
		config::app_name, nullptr, nullptr));

	glfwSetWindowUserPointer(m_window.get(), this);
	glfwSetWindowSizeCallback(m_window.get(), RenderManager::onWindowsResized);

}

auto RenderManager::initVulkan() noexcept(false) -> void {
	m_instance = createInstance();
	setupDebugCallback();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createSwapChain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createGraphicsCommandPool();
	createVertexBuffer();
	createCommandBuffers();
	recordCommandBuffers();
	createSemaphores();
}

auto RenderManager::recreateSwapChain() -> void {
	auto width = 0;
	auto height = 0;
	glfwGetWindowSize(m_window.get(), &width, &height);
	/*
	When the window is minimized we don't need to recreate the swap chain.
	In those cases the width and height will be 0.
	*/
	if (width == 0 || height == 0) {
		return;
	}

	vkDeviceWaitIdle(m_device);


	/*
	@Optimization: Create the new swapchain using the old one with the
	field "oldSwapChain" so we don't have to stop rendering.
	*/
	cleanupSwapChain();

	createSwapChain();
	createImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createFramebuffers();
	createCommandBuffers();
	recordCommandBuffers();
}


auto RenderManager::loopIteration() noexcept -> void {
	glfwPollEvents();
	drawFrame();
}

auto RenderManager::cleanup() noexcept -> void {
	cleanupSwapChain();


	/**
	@TODO @DOING: Check here why the buffer is still in use
	*/
	vkDestroyBuffer(m_device, m_vertex_buffer, nullptr);
	vkFreeMemory(m_device, m_vertex_buffer_memory, nullptr);

	vkDestroySemaphore(m_device, m_image_available_semaphore, nullptr);
	vkDestroySemaphore(m_device, m_render_finished_semaphore, nullptr);

	vkDestroyCommandPool(m_device, m_graphics_command_pool, nullptr);

	vkDestroyDevice(m_device, nullptr);

	destroyDebugReportCallbackEXT(m_instance, m_debug_callback, nullptr);

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

	vkDestroyInstance(m_instance, nullptr);

	m_window.reset();

	glfwTerminate();
}

auto RenderManager::cleanupSwapChain() noexcept -> void {

	for (const auto& framebuffer : m_swap_chain_framebuffers) {
		vkDestroyFramebuffer(m_device, framebuffer, nullptr);
	}

	vkFreeCommandBuffers(
		m_device,
		m_graphics_command_pool,
		gsl::narrow_cast<uint>(m_command_buffers.size()),
		m_command_buffers.data());

	vkDestroyPipeline(m_device, m_pipeline, nullptr);
	vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);

	vkDestroyRenderPass(m_device, m_render_pass, nullptr);

	for (const auto& view : m_swap_chain_image_views) {
		vkDestroyImageView(m_device, view, nullptr);
	}

	vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);

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

		std::cout << "Printing available extensions in the current vulkan instance" << std::endl;
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

	if (extensions.size() == 0) {
		std::cout << "\tNo available extensions" << std::endl;
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

	std::cerr << "\tVALIDATION LAYER MESSAGE [" << layer_prefix << "]: ";

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

	vkGetPhysicalDeviceProperties(m_physical_device, &m_physical_device_properties);
	vkGetPhysicalDeviceFeatures(m_physical_device, &m_physical_device_features);

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
	if (!features.geometryShader ||
		!family_indices.isComplete() ||
		!device_extensions_supported ||
		!swap_chain_suitable) {
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
				if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
					indices.graphics_family = i;
				}
				if (family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
					indices.transfer_family = i;
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
	auto unique_queue_families = std::set<int>{
		m_queue_family_indices.graphics_family,
		m_queue_family_indices.present_family,
		m_queue_family_indices.transfer_family };

	const auto queue_priority = 1.0f;

	for (auto queue_familie : unique_queue_families) {
		auto queue_create_info = VkDeviceQueueCreateInfo{};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
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
		auto width = 0;
		auto height = 0;

		glfwGetWindowSize(m_window.get(), &width, &height);

		auto extent = VkExtent2D{
			gsl::narrow<uint>(width),
			gsl::narrow<uint>(height)
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
		gsl::narrow<uint>(m_queue_family_indices.present_family) ,
		gsl::narrow<uint>(m_queue_family_indices.transfer_family)
	};

	std::sort(queue_family_indices.begin(), queue_family_indices.end());
	queue_family_indices.erase(
		std::unique(queue_family_indices.begin(), queue_family_indices.end()),
		queue_family_indices.end());

	create_info.imageSharingMode = queue_family_indices.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE;
	create_info.queueFamilyIndexCount = gsl::narrow<uint>(queue_family_indices.size());
	create_info.pQueueFamilyIndices = queue_family_indices.data();


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

auto RenderManager::createImageViews() -> void {

	m_swap_chain_image_views.resize(m_swap_chain_images.size());

	for (size_t i = 0; i < m_swap_chain_image_views.size(); i++) {
		auto create_info = VkImageViewCreateInfo{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.flags;
		create_info.image = m_swap_chain_images[i];
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = m_swap_chain_image_format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = 1;

		if (vkCreateImageView(m_device, &create_info, nullptr, &m_swap_chain_image_views[i]) != VK_SUCCESS) {
			throw std::runtime_error("Couldn't create an image view for an image in the swap chain");
		}
	}
}

auto RenderManager::createRenderPass() -> void {

	std::cout << "Creating Render Pass" << std::endl;


	auto color_attachment_description = VkAttachmentDescription{};
	color_attachment_description.format = m_swap_chain_image_format;
	/*
	This is also relevant for implementing multisampling
	*/
	color_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
	color_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	color_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	color_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	color_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
	color_attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

	auto color_attachment_reference = VkAttachmentReference{};
	color_attachment_reference.attachment = 0;
	color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	auto subpass_description = VkSubpassDescription{};
	subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass_description.colorAttachmentCount = 1;
	subpass_description.pColorAttachments = &color_attachment_reference;

	auto subpass_dependency = VkSubpassDependency{};
	subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
	subpass_dependency.dstSubpass = 0;
	subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpass_dependency.srcAccessMask = 0;
	subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
	subpass_dependency.dstAccessMask =
		VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
		VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;


	auto render_pass_create_info = VkRenderPassCreateInfo{};
	render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	render_pass_create_info.attachmentCount = 1;
	render_pass_create_info.pAttachments = &color_attachment_description;
	render_pass_create_info.subpassCount = 1;
	render_pass_create_info.pSubpasses = &subpass_description;
	render_pass_create_info.dependencyCount = 1;
	render_pass_create_info.pDependencies = &subpass_dependency;

	if (vkCreateRenderPass(m_device, &render_pass_create_info, nullptr, &m_render_pass) != VK_SUCCESS) {
		throw std::runtime_error("We could't create a render pass");
	}

	std::cout << "\tRender Pass Created" << std::endl << std::endl;
}


#include "./shaders/triangle_frag.hpp"
#include "./shaders/triangle_vert.hpp"


auto RenderManager::createGraphicsPipeline() -> void {

	auto vert_shader_code = readBinaryArrayToChars(triangle_vert);
	auto frag_shader_code = readBinaryArrayToChars(triangle_frag);

	auto vert_shader_module = createShaderModule(vert_shader_code);

	auto frag_shader_module = createShaderModule(frag_shader_code);


	std::cout << "Creating Graphics Pipeline" << std::endl;

	auto vert_shader_stage_info = VkPipelineShaderStageCreateInfo{};
	vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
	vert_shader_stage_info.module = vert_shader_module;
	vert_shader_stage_info.pName = "main";
	/*
	The attribute "vert_shader_stage_info.pSpecializationInfo" can be used to set
	values for shader constants to modify shader behaviour. This is much more eficient
	than using input variables for the shaders in render time since the compiler
	can use these constants for optimization purposes.

	@see vert_shader_stage_info.pSpecializationInfo
	*/

	auto frag_shader_stage_info = VkPipelineShaderStageCreateInfo{};
	frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	frag_shader_stage_info.module = frag_shader_module;
	frag_shader_stage_info.pName = "main";


	const VkPipelineShaderStageCreateInfo shader_stages[] =
	{ vert_shader_stage_info, frag_shader_stage_info };

	/* We will modify this later when we deal with the vertex buffer */

	auto binding_descriptions = Vertex::getBindingDescription();
	auto attribute_descriptions = Vertex::getAttributeDescriptions();

	auto vertex_input_create_info = VkPipelineVertexInputStateCreateInfo{};
	vertex_input_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_input_create_info.vertexBindingDescriptionCount = 1;
	vertex_input_create_info.pVertexBindingDescriptions = &binding_descriptions;
	vertex_input_create_info.vertexAttributeDescriptionCount = gsl::narrow_cast<uint>(attribute_descriptions.size());
	vertex_input_create_info.pVertexAttributeDescriptions = attribute_descriptions.data();

	auto input_assembly_create_info = VkPipelineInputAssemblyStateCreateInfo{};
	input_assembly_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
	input_assembly_create_info.primitiveRestartEnable = VK_FALSE;

	auto viewport = VkViewport{};
	viewport.x = 0.0f;
	viewport.y = 0.0f;
	viewport.width = gsl::narrow<float>(m_swap_chain_extent.width);
	viewport.height = gsl::narrow<float>(m_swap_chain_extent.height);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;

	auto scissor = VkRect2D{};
	scissor.offset = { 0, 0 };
	scissor.extent = VkExtent2D{ m_swap_chain_extent };


	auto viewport_create_info = VkPipelineViewportStateCreateInfo{};
	viewport_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_create_info.viewportCount = 1;
	viewport_create_info.pViewports = &viewport;
	viewport_create_info.scissorCount = 1;
	viewport_create_info.pScissors = &scissor;

	auto rasterizer_create_info = VkPipelineRasterizationStateCreateInfo{};
	rasterizer_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer_create_info.depthClampEnable = VK_FALSE;
	rasterizer_create_info.rasterizerDiscardEnable = VK_FALSE;
	/*
	Other options for polygonMode are LINE or POINT but these require a GPU feature
	*/
	rasterizer_create_info.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizer_create_info.lineWidth = 1.0f;
	rasterizer_create_info.cullMode = VK_CULL_MODE_BACK_BIT;
	rasterizer_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
	rasterizer_create_info.depthBiasEnable = VK_FALSE;
	rasterizer_create_info.depthBiasConstantFactor = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor = 0.0f;


	auto multisampling_create_info = VkPipelineMultisampleStateCreateInfo{};
	multisampling_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	/*
	We could activate 4XMSAA if we support it with the following code:

	if ((m_physical_device_properties.limits.framebufferColorSampleCounts & VK_SAMPLE_COUNT_4_BIT) != 0) {

		multisampling_create_info.rasterizationSamples = VK_SAMPLE_COUNT_4_BIT;

		. . .
	}else{	// No MSAA

		//What we already do

	}

	@see createRenderPass
	*/
	multisampling_create_info.sampleShadingEnable = VK_FALSE;
	multisampling_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	multisampling_create_info.minSampleShading = 1.0f;
	multisampling_create_info.pSampleMask = nullptr;
	multisampling_create_info.alphaToCoverageEnable = VK_FALSE;
	multisampling_create_info.alphaToOneEnable = VK_FALSE;

	/*
	We can use:

	auto depth_stencil_create_info = VkPipelineDepthStencilStateCreateInfo{};

	When we want to use a depth or stencil buffer.
	*/

	auto color_blend_attachment_state = VkPipelineColorBlendAttachmentState{};
	color_blend_attachment_state.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT |
		VK_COLOR_COMPONENT_G_BIT |
		VK_COLOR_COMPONENT_B_BIT |
		VK_COLOR_COMPONENT_A_BIT;
	/*
	We set the parameters for alpha blending
	*/
	color_blend_attachment_state.blendEnable = VK_TRUE;
	color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
	color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
	color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

	auto color_blend_create_info = VkPipelineColorBlendStateCreateInfo{};
	color_blend_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend_create_info.logicOpEnable = VK_FALSE;
	color_blend_create_info.logicOp = VK_LOGIC_OP_COPY;
	color_blend_create_info.attachmentCount = 1;
	color_blend_create_info.pAttachments = &color_blend_attachment_state;
	color_blend_create_info.blendConstants[0] = 0.0f;
	color_blend_create_info.blendConstants[1] = 0.0f;
	color_blend_create_info.blendConstants[2] = 0.0f;
	color_blend_create_info.blendConstants[3] = 0.0f;


	const VkDynamicState dynamic_states[] = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_LINE_WIDTH
	};

	auto dynamic_state_create_info = VkPipelineDynamicStateCreateInfo{};
	dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state_create_info.dynamicStateCount = 2;
	[[gsl::suppress(bounds.3)]]{
	dynamic_state_create_info.pDynamicStates = dynamic_states;
	}

	auto pipeline_layout_create_info = VkPipelineLayoutCreateInfo{};
	pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_create_info.setLayoutCount = 0;
	pipeline_layout_create_info.pSetLayouts = nullptr;
	pipeline_layout_create_info.pushConstantRangeCount = 0;
	pipeline_layout_create_info.pPushConstantRanges = nullptr;

	if (vkCreatePipelineLayout(m_device, &pipeline_layout_create_info, nullptr, &m_pipeline_layout) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a pipeline layout");
	}

	auto pipeline_create_info = VkGraphicsPipelineCreateInfo{};
	pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipeline_create_info.stageCount = 2;
	[[gsl::suppress(bounds.3)]]{
	pipeline_create_info.pStages = shader_stages;
	}
	pipeline_create_info.pVertexInputState = &vertex_input_create_info;
	pipeline_create_info.pInputAssemblyState = &input_assembly_create_info;
	pipeline_create_info.pViewportState = &viewport_create_info;
	pipeline_create_info.pRasterizationState = &rasterizer_create_info;
	pipeline_create_info.pMultisampleState = &multisampling_create_info;
	pipeline_create_info.pDepthStencilState = nullptr;
	pipeline_create_info.pColorBlendState = &color_blend_create_info;
	pipeline_create_info.pDynamicState = nullptr;
	pipeline_create_info.layout = m_pipeline_layout;
	pipeline_create_info.renderPass = m_render_pass;
	pipeline_create_info.subpass = 0;
	pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
	pipeline_create_info.basePipelineIndex = -1;

	if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_create_info, nullptr, &m_pipeline)
		!= VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a graphics pipeline");
	}
	std::cout << "\tGraphics Pipeline Created" << std::endl << std::endl;

	vkDestroyShaderModule(m_device, vert_shader_module, nullptr);
	vkDestroyShaderModule(m_device, frag_shader_module, nullptr);

}

auto RenderManager::createShaderModule(const std::vector<char>& code) const -> VkShaderModule {
	std::cout << "Creating Shader Module " << std::endl;

	auto create_info = VkShaderModuleCreateInfo{};
	create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	create_info.codeSize = code.size();
	[[gsl::suppress(type.1)]]{
	create_info.pCode = reinterpret_cast<const uint*>(code.data());
	}
	auto shader_module = VkShaderModule{};

	if (vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a shader module");
	}

	std::cout << "\tShader Module Created" << std::endl << std::endl;

	return shader_module;

}

auto RenderManager::createFramebuffers() ->  void {

	std::cout << "Creating Framebuffers " << std::endl;


	m_swap_chain_framebuffers.resize(m_swap_chain_image_views.size());

	for (size_t i = 0; i < m_swap_chain_image_views.size(); ++i) {
		VkImageView attachments[] = { m_swap_chain_image_views[i] };

		auto frame_buffer_create_info = VkFramebufferCreateInfo{};
		frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frame_buffer_create_info.renderPass = m_render_pass;
		frame_buffer_create_info.attachmentCount = 1;
		[[gsl::suppress(bounds.3)]]{
		frame_buffer_create_info.pAttachments = attachments;
		}
		frame_buffer_create_info.width = m_swap_chain_extent.width;
		frame_buffer_create_info.height = m_swap_chain_extent.height;
		frame_buffer_create_info.layers = 1;

		if (vkCreateFramebuffer(m_device,
			&frame_buffer_create_info,
			nullptr,
			&m_swap_chain_framebuffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("We couldn't create a necessary frame buffer");
		}

	}

	std::cout << "\tFramebuffers Created" << std::endl << std::endl;


}

auto RenderManager::createGraphicsCommandPool() ->  void {
	std::cout << "Creating Graphics Command Pool " << std::endl;

	auto command_pool_create_info = VkCommandPoolCreateInfo{};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = m_queue_family_indices.graphics_family;
	/*
	For the flags we could use:

		VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: If we are going to rerecord the command
		buffer with new commands frequently

		VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: If we want to rerecord command
		buffers individually (without resetting all together).

	*/
	command_pool_create_info.flags = 0;

	if (vkCreateCommandPool(m_device, &command_pool_create_info, nullptr, &m_graphics_command_pool) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a graphics command pool");
	}

	std::cout << "\tGraphics Command Pool Created" << std::endl << std::endl;
}

auto RenderManager::createTransferCommandPool() ->  void {
	std::cout << "Creating Transfer Command Pool " << std::endl;

	auto command_pool_create_info = VkCommandPoolCreateInfo{};
	command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	command_pool_create_info.queueFamilyIndex = m_queue_family_indices.transfer_family;
	/*
	For the flags we could use:

	VK_COMMAND_POOL_CREATE_TRANSIENT_BIT: If we are going to rerecord the command
	buffer with new commands frequently

	VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT: If we want to rerecord command
	buffers individually (without resetting all together).

	*/
	command_pool_create_info.flags = 0;

	if (vkCreateCommandPool(m_device, &command_pool_create_info, nullptr, &m_transfer_command_pool) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a transfer command pool");
	}

	std::cout << "\tTransfer Command Pool Created" << std::endl << std::endl;
}

auto RenderManager::createBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
	VkMemoryPropertyFlags properties,
	VkSharingMode sharing_mode,
	const std::vector<uint>* queue_family_indices,
	VkBuffer& buffer,
	VkDeviceMemory& buffer_memory) -> void {

	auto buffer_create_info = VkBufferCreateInfo{};

	buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_create_info.size = size;
	buffer_create_info.usage = usage;

	buffer_create_info.sharingMode = sharing_mode;
	if (sharing_mode == VK_SHARING_MODE_CONCURRENT) {
		if (queue_family_indices == nullptr || queue_family_indices->size() <= 1) {
			throw std::runtime_error("We can't create a shared buffer without more than one families to share between");
		}
		buffer_create_info.queueFamilyIndexCount = gsl::narrow<uint>(queue_family_indices->size());
		buffer_create_info.pQueueFamilyIndices = queue_family_indices->data();
	}
	else {
		buffer_create_info.queueFamilyIndexCount = 0;
		buffer_create_info.pQueueFamilyIndices = nullptr;
	}

	if (vkCreateBuffer(m_device, &buffer_create_info, nullptr, &buffer) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a buffer");
	}

	auto memory_requirements = VkMemoryRequirements{};
	vkGetBufferMemoryRequirements(m_device, buffer, &memory_requirements);

	auto allocate_info = VkMemoryAllocateInfo{};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.allocationSize = memory_requirements.size;
	allocate_info.memoryTypeIndex = findMemoryType(
		memory_requirements.memoryTypeBits,
		properties);

	if (vkAllocateMemory(m_device, &allocate_info, nullptr, &buffer_memory) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't allocate the required memory for a buffer");
	}

	vkBindBufferMemory(m_device, m_vertex_buffer, buffer_memory, 0);
}

auto RenderManager::createVertexBuffer() -> void {

	/*
	Access to this buffer will be granted to both the graphics family to allow the
	GPU to render from it and the transfer family so we can write to the buffer
	from another one mapped to CPU memory.
	*/
	auto queue_family_indices = std::vector<uint>{
		gsl::narrow<uint>(m_queue_family_indices.graphics_family) ,
		gsl::narrow<uint>(m_queue_family_indices.transfer_family)
	};

	std::sort(queue_family_indices.begin(), queue_family_indices.end());
	queue_family_indices.erase(
		std::unique(queue_family_indices.begin(), queue_family_indices.end()),
		queue_family_indices.end());

	auto buffer_size = VkDeviceSize{ sizeof(vertices)*vertices.size() };
	createBuffer(
		buffer_size,
		VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
		VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
		queue_family_indices.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
		&queue_family_indices,
		m_vertex_buffer,
		m_vertex_buffer_memory);


	void* data;
	vkMapMemory(m_device, m_vertex_buffer_memory, 0, buffer_size, 0, &data);
	memcpy(data, vertices.data(), gsl::narrow_cast<size_t>(buffer_size));
	vkUnmapMemory(m_device, m_vertex_buffer_memory);

}


auto RenderManager::findMemoryType(uint type_filter, VkMemoryPropertyFlags properties)->uint {

	auto memory_properties = VkPhysicalDeviceMemoryProperties{};
	vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);

	for (uint i = 0; memory_properties.memoryTypeCount; ++i) {
		if ((type_filter & (1 << i)) &&
			(memory_properties.memoryTypes[i].propertyFlags & properties) == properties) {
			return i;
		}
	}

	throw std::runtime_error("We couldn't find an appropriate memory type");
}

auto RenderManager::createCommandBuffers() ->  void {
	std::cout << "Creating Command Buffers " << std::endl;

	m_command_buffers.resize(m_swap_chain_framebuffers.size());

	auto alloc_info = VkCommandBufferAllocateInfo{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.commandPool = m_graphics_command_pool;
	/*
	Level can have this values with this meaning:

	VK_COMMAND_BUFFER_LEVEL_PRIMARY: Means that it can be executed directly
	in a queue but it CANNOT be called from another command buffer.

	VK_COMMAND_BUFFER_LEVEL_SECONDARY: Means that it CANNOT be executed in a
	queue directly but it can be called from other primary command buffers.
	*/
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = gsl::narrow<uint>(m_command_buffers.size());

	if (vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.data()) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't allocate the necessary command buffers");
	}

	std::cout << "\tCommand Buffers Created" << std::endl << std::endl;

}

auto RenderManager::recordCommandBuffers() -> void {
	std::cout << "Recording Command Buffers " << std::endl;


	for (size_t i = 0; i < m_command_buffers.size(); ++i) {
		auto begin_info = VkCommandBufferBeginInfo{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
		begin_info.pInheritanceInfo = nullptr;

		vkBeginCommandBuffer(m_command_buffers[i], &begin_info);

		auto render_info = VkRenderPassBeginInfo{};
		render_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_info.renderPass = m_render_pass;
		render_info.framebuffer = m_swap_chain_framebuffers[i];
		render_info.renderArea.offset = { 0, 0 };
		render_info.renderArea.extent = m_swap_chain_extent;
		render_info.clearValueCount = 1;
		render_info.pClearValues = &config::clear_color;

		vkCmdBeginRenderPass(m_command_buffers[i], &render_info, VK_SUBPASS_CONTENTS_INLINE);

		{
			vkCmdBindPipeline(m_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

			VkBuffer vertex_buffers[] = { m_vertex_buffer };
			VkDeviceSize offsets[] = { 0 };
			vkCmdBindVertexBuffers(m_command_buffers[i], 0, 1, vertex_buffers, offsets);

			vkCmdDraw(m_command_buffers[i], gsl::narrow<uint>(vertices.size()), 1, 0, 0);
		}

		vkCmdEndRenderPass(m_command_buffers[i]);

		if (vkEndCommandBuffer(m_command_buffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("We couldn't record a command buffer");
		}

	}

	std::cout << "\tCommand Buffers Recorded" << std::endl << std::endl;
}

auto RenderManager::createSemaphores() -> void {

	auto create_info = VkSemaphoreCreateInfo{};
	create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

	if ((vkCreateSemaphore(m_device, &create_info, nullptr, &m_image_available_semaphore) != VK_SUCCESS) ||
		(vkCreateSemaphore(m_device, &create_info, nullptr, &m_render_finished_semaphore) != VK_SUCCESS)) {

		throw std::runtime_error("We couldn't create the semaphores necessary for rendering");

	}


}

auto RenderManager::drawFrame() -> void {
	uint image_index;

	vkQueueWaitIdle(m_present_queue);

	auto result = vkAcquireNextImageKHR(
		m_device,
		m_swap_chain,
		/*
		This is the timeout in ns for an image to become available
		*/
		std::numeric_limits<uint64_t>::max(),
		m_image_available_semaphore,
		VK_NULL_HANDLE,
		&image_index);

	if (result == VK_ERROR_OUT_OF_DATE_KHR) {
		recreateSwapChain();
		return;
	}
	else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
		throw std::runtime_error("We couldn't acquire an image to render to");
	}

	auto submit_info = VkSubmitInfo{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

	const VkSemaphore wait_semaphores[] = { m_image_available_semaphore };
	const VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
	submit_info.waitSemaphoreCount = 1;
	submit_info.pWaitSemaphores = wait_semaphores;
	submit_info.pWaitDstStageMask = wait_stages;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &m_command_buffers[image_index];
	VkSemaphore signal_semaphores[] = { m_render_finished_semaphore };
	submit_info.signalSemaphoreCount = 1;
	submit_info.pSignalSemaphores = signal_semaphores;


	if (vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't submit our command buffer");
	}

	auto present_info = VkPresentInfoKHR{};
	present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
	present_info.waitSemaphoreCount = 1;
	present_info.pWaitSemaphores = signal_semaphores;
	VkSwapchainKHR swap_chains[] = { m_swap_chain };
	present_info.swapchainCount = 1;
	present_info.pSwapchains = swap_chains;
	present_info.pImageIndices = &image_index;
	present_info.pResults = nullptr;


	result = vkQueuePresentKHR(m_present_queue, &present_info);

	if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
		recreateSwapChain();
	}
	else if (result != VK_SUCCESS) {
		throw std::runtime_error("We couldn't submit the presentation info to the queue");
	}

}

auto RenderManager::onWindowsResized(GLFWwindow * window, int width, int height) -> void {

	RenderManager* render_manager = nullptr;

	[[gsl::suppress(type.1)]]{
		render_manager = reinterpret_cast<RenderManager*>(glfwGetWindowUserPointer(window));
	}

	render_manager->recreateSwapChain();

	std::cout << " - Window resized to (" << width << ", " << height << ")" << std::endl;
}



