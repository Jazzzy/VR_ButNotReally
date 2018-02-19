#include "Renderer.h"
#include "../Configuration.h"
#include <vulkan/vk_platform.h>
#include <limits>
#include <map>
#include <set>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <CppCoreCheck/Warnings.h>


#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

/*
Include section without warnings from GSL
*/
#pragma warning(push)
#pragma warning(disable: ALL_CPPCORECHECK_WARNINGS)

#ifdef VMA_USE_ALLOCATOR
#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"
#endif

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#pragma warning(disable: 6001 6308 6262 6387 28182)

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#pragma warning(pop)


/*
We include the compiled shader code we are going to use.
*/
#include "./shaders/triangle_frag.hpp"
#include "./shaders/triangle_vert.hpp"

Renderer::Renderer() : m_instance() {
	initWindow();
	initVulkan();
};

Renderer::~Renderer() {
	cleanup();
};

auto Renderer::shouldClose() const noexcept -> bool {
	return glfwWindowShouldClose(m_window.get());
}

auto Renderer::initWindow() noexcept -> void {
	glfwInit();
	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	m_window = WindowPtr(glfwCreateWindow(
		config::initial_window_width,
		config::initial_window_heigth,
		config::app_name, nullptr, nullptr));

	glfwSetWindowUserPointer(m_window.get(), this);
	glfwSetWindowSizeCallback(m_window.get(), Renderer::onWindowsResized);

}

auto Renderer::initVulkan() noexcept(false) -> void {
	m_instance = createInstance();
	setupDebugCallback();
	createSurface();
	pickPhysicalDevice();
	createLogicalDevice();
	createAllocator();
	createSwapChain();
	createSwapChainImageViews();
	createRenderPass();
	createDescriptorSetLayout();
	createGraphicsPipeline();
	createGraphicsCommandPool();
	createTransferCommandPool();
	createDepthResources();
	createFramebuffers();
	loadScene(
		config::model_path + "obj/tarzan/Tarzan_packed/tarzan_scaled.obj",
		config::model_path + "obj/tarzan/Tarzan_packed/Tarzan_packed_full.png");
	createTextureSampler();
	createVertexBuffer();
	createIndexBuffer();
	createUniformBuffer();
	createDescriptorPool();
	createDescriptorSet();
	createCommandBuffers();
	recordCommandBuffers();
	createSemaphoresAndFences();
}

auto Renderer::recreateSwapChain() -> void {
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
	createSwapChainImageViews();
	createRenderPass();
	createGraphicsPipeline();
	createDepthResources();
	createFramebuffers();
	createCommandBuffers();
	recordCommandBuffers();

	m_current_swapchain_buffer = 0;
	m_current_command_buffer = 0;
}

auto Renderer::cleanup() noexcept -> void {
	vkDeviceWaitIdle(m_device);

	cleanupSwapChain();

	vkDestroySampler(m_device, m_texture_sampler, nullptr);

	vkDestroyImageView(m_device, m_scene.m_texture_image_view, nullptr);
	destroyImage(m_scene.m_texture_image);

	/*
	This also frees the memory of the descriptor sets it contains
	*/
	vkDestroyDescriptorPool(m_device, m_descriptor_pool, nullptr);

	vkDestroyDescriptorSetLayout(m_device, m_descriptor_set_layout, nullptr);
	destroyBuffer(m_uniform_buffer);

	destroyBuffer(m_index_buffer);
	destroyBuffer(m_vertex_buffer);

	for (auto i = 0; i < m_command_buffers.size(); ++i) {
		vkDestroyFence(m_device, m_command_buffer_fences[i], nullptr);
	}

	for (auto i = 0; i < m_swap_chain_images.size(); ++i) {
		vkDestroySemaphore(m_device, m_image_available_semaphores[i], nullptr);
	}

	for (auto i = 0; i < m_command_buffers.size(); ++i) {
		vkDestroySemaphore(m_device, m_render_finished_semaphores[i], nullptr);
	}

	vkDestroyCommandPool(m_device, m_graphics_command_pool, nullptr);
	vkDestroyCommandPool(m_device, m_transfer_command_pool, nullptr);

#ifdef VMA_USE_ALLOCATOR
	vmaDestroyAllocator(m_vma_allocator);
#endif

	vkDestroyDevice(m_device, nullptr);

	destroyDebugReportCallbackEXT(m_instance, m_debug_callback, nullptr);

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

	vkDestroyInstance(m_instance, nullptr);

	m_window.reset();

	glfwTerminate();
}

auto Renderer::cleanupSwapChain() noexcept -> void {

	vkDestroyImageView(m_device, m_depth_image_view, nullptr);
	destroyImage(m_depth_image);

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

	if (m_render_target.init) {
		vkDestroyImageView(m_device, m_render_target.view, nullptr);
		vkDestroyImage(m_device, m_render_target.image, nullptr);
		vkFreeMemory(m_device, m_render_target.memory, nullptr);
	}

	if (m_depth_target.init) {
		vkDestroyImageView(m_device, m_depth_target.view, nullptr);
		vkDestroyImage(m_device, m_depth_target.image, nullptr);
		vkFreeMemory(m_device, m_depth_target.memory, nullptr);
	}

}

auto Renderer::createInstance() noexcept(false) -> VkInstance {

	std::cout << "Creating Vulkan Instance" << std::endl;

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

	std::cout << "\tVulkan Instance Created" << std::endl << std::endl;

	return instance;
}

[[gsl::suppress(bounds)]] auto Renderer::printInstanceExtensions(const std::vector<VkExtensionProperties>& extensions) const -> void {

	if (extensions.size() == 0) {
		std::cout << "\tNo available extensions" << std::endl;
		return;
	}

	for (const auto& e : extensions) {
		std::cout << "\t[" << e.extensionName << "]" << std::endl;
	}

	std::cout << std::endl;

}

auto Renderer::checkInstanceExtensionsNamesAvailable(const std::vector<const char*>& required_extensions, const std::vector<VkExtensionProperties>& available_extensions) const -> bool {

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

auto Renderer::checkValidationLayerSupport() const noexcept -> bool {

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

[[gsl::suppress(bounds.1)]] auto Renderer::getRequiredExtensions() const noexcept -> std::vector<const char*> {

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
auto VKAPI_ATTR VKAPI_CALL Renderer::debugReportCallback(
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

auto Renderer::setupDebugCallback() -> void {

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

auto Renderer::createDebugReportCallbackEXT(
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

auto Renderer::destroyDebugReportCallbackEXT(
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

auto Renderer::createSurface() -> void {

	std::cout << "Creating Surface" << std::endl;


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

	std::cout << "\tSurface Created" << std::endl << std::endl;

}

auto Renderer::pickPhysicalDevice() -> void {

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

auto Renderer::physicalDeviceSuitability(const VkPhysicalDevice & device) const noexcept -> std::tuple<bool, int> {

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
		!swap_chain_suitable ||
		!features.samplerAnisotropy) {
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

auto Renderer::findQueueFamilies(const VkPhysicalDevice& physical_device, PrintOptions print_options) const -> QueueFamilyIndices {

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

auto Renderer::createLogicalDevice() -> void {

	std::cout << "Creating Logical Device" << std::endl;


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
	auto physical_device_features = VkPhysicalDeviceFeatures{};
	physical_device_features.samplerAnisotropy = VK_TRUE;

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
	vkGetDeviceQueue(m_device, m_queue_family_indices.transfer_family, 0, &m_transfer_queue);

	std::cout << "\tLogical Device Created" << std::endl << std::endl;

}

auto Renderer::createAllocator() noexcept ->void {
#ifdef VMA_USE_ALLOCATOR
	std::cout << "Creating Allocator" << std::endl;

	auto create_info = VmaAllocatorCreateInfo{};
	create_info.physicalDevice = m_physical_device;
	create_info.device = m_device;

	vmaCreateAllocator(&create_info, &m_vma_allocator);

	std::cout << "\tAllocator Created" << std::endl << std::endl;
#endif
}

auto Renderer::checkDeviceExtensionSupport(const VkPhysicalDevice& device) const -> bool {

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

auto Renderer::querySwapChainSupport(const VkPhysicalDevice& device) const->SwapChainSupportDetails {

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

auto Renderer::pickSurfaceChainFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const ->VkSurfaceFormatKHR {


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

auto Renderer::pickSurfacePresentMode(const std::vector<VkPresentModeKHR>& available_modes)  const noexcept -> VkPresentModeKHR {


	for (const auto& desired_mode : config::preferred_present_modes_sorted) {
		for (const auto& available_mode : available_modes) {
			if (desired_mode == available_mode) return desired_mode;
		}
	}


	return VK_PRESENT_MODE_FIFO_KHR;	// We are guaranteed that this mode is supported, so in theory we should never reach this point.
}

auto Renderer::pickSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)  const noexcept -> VkExtent2D {

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

auto Renderer::createSwapChain() -> void {

	std::cout << "Creating Swap Chain" << std::endl;


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

	if (config.multisampling_samples != 1) {
		m_render_target = createMultisampleRenderTarget(
			extent.width,
			extent.height,
			surface_format.format,
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			VK_IMAGE_ASPECT_COLOR_BIT);

		m_depth_target = createMultisampleRenderTarget(
			extent.width,
			extent.height,
			findDepthFormat(),
			VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
			VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT);
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

	std::cout << "\tSwap Chain Created" << std::endl << std::endl;

}

auto Renderer::createSwapChainImageViews() -> void {

	std::cout << "Creating Image Views" << std::endl;


	m_swap_chain_image_views.resize(m_swap_chain_images.size());

	for (size_t i = 0; i < m_swap_chain_image_views.size(); i++) {
		m_swap_chain_image_views[i] = createImageView(
			m_swap_chain_images[i],
			m_swap_chain_image_format,
			VK_IMAGE_ASPECT_COLOR_BIT);
	}

	std::cout << "\tImage Views Created" << std::endl << std::endl;
}

auto Renderer::createRenderPass() -> void {

	std::cout << "Creating Render Pass" << std::endl;

	if (config.multisampling_samples == 1) {

		auto render_pass_create_info = VkRenderPassCreateInfo{};
		auto attachment_descriptions = std::array<VkAttachmentDescription, 2>{};

		attachment_descriptions[0].format = m_swap_chain_image_format;
		attachment_descriptions[0].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		attachment_descriptions[1].format = findDepthFormat();
		attachment_descriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		auto color_attachment_reference = VkAttachmentReference{};
		color_attachment_reference.attachment = 0;
		color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		auto depth_attachment_reference = VkAttachmentReference{};
		depth_attachment_reference.attachment = 1;
		depth_attachment_reference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		auto subpass_description = VkSubpassDescription{};
		subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass_description.colorAttachmentCount = 1;
		subpass_description.pColorAttachments = &color_attachment_reference;
		subpass_description.pDepthStencilAttachment = &depth_attachment_reference;

		auto subpass_dependency = VkSubpassDependency{};
		subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		subpass_dependency.dstSubpass = 0;
		subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependency.srcAccessMask = 0;
		subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependency.dstAccessMask =
			VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
			VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

		render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_create_info.attachmentCount = gsl::narrow_cast<uint>(attachment_descriptions.size());
		render_pass_create_info.pAttachments = attachment_descriptions.data();
		render_pass_create_info.subpassCount = 1;
		render_pass_create_info.pSubpasses = &subpass_description;
		render_pass_create_info.dependencyCount = 1;
		render_pass_create_info.pDependencies = &subpass_dependency;

		if (vkCreateRenderPass(m_device, &render_pass_create_info, nullptr, &m_render_pass) != VK_SUCCESS) {
			throw std::runtime_error("We could't create a render pass");
		}
	}
	else {
		auto attachment_descriptions = std::array<VkAttachmentDescription, 4>{};


		/*
		We set up the attachments for the render pass

		https://github.com/SaschaWillems/Vulkan/blob/master/examples/multisampling/multisampling.cpp @ 247
		*/

		/*
		Multisampled render target
		*/
		attachment_descriptions[0].format = m_swap_chain_image_format;
		attachment_descriptions[0].samples = getSampleBits(config.multisampling_samples);
		attachment_descriptions[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		/*
		FrameBuffer we resolve the multisampled image to
		*/
		attachment_descriptions[1].format = m_swap_chain_image_format;
		attachment_descriptions[1].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[1].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[1].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		/*
		Multisampled depth attachment
		*/
		attachment_descriptions[2].format = findDepthFormat();
		attachment_descriptions[2].samples = getSampleBits(config.multisampling_samples);
		attachment_descriptions[2].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		attachment_descriptions[2].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[2].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[2].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[2].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[2].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		/*
		FrameBuffer we resolve the multisampled image to
		*/
		attachment_descriptions[3].format = findDepthFormat();
		attachment_descriptions[3].samples = VK_SAMPLE_COUNT_1_BIT;
		attachment_descriptions[3].loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[3].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		attachment_descriptions[3].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		attachment_descriptions[3].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		attachment_descriptions[3].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		attachment_descriptions[3].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		const auto color_attachment_reference = VkAttachmentReference{ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };
		const auto depth_attachment_reference = VkAttachmentReference{ 2, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL };
		const auto resolve_attachment_reference = VkAttachmentReference{ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };


		auto subpass_description = VkSubpassDescription{ 0 };
		subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass_description.colorAttachmentCount = 1;
		subpass_description.pColorAttachments = &color_attachment_reference;
		subpass_description.pResolveAttachments = &resolve_attachment_reference;
		subpass_description.pDepthStencilAttachment = &depth_attachment_reference;


		auto subpass_dependencies = std::array<VkSubpassDependency, 2>{};
		subpass_dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
		subpass_dependencies[0].dstSubpass = 0;
		subpass_dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		subpass_dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		subpass_dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		subpass_dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

		subpass_dependencies[1].srcSubpass = 0;
		subpass_dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
		subpass_dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
		subpass_dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
		subpass_dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_READ_BIT;
		subpass_dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
		subpass_dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;


		auto render_pass_create_info = VkRenderPassCreateInfo{};
		render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		render_pass_create_info.attachmentCount = gsl::narrow_cast<uint>(attachment_descriptions.size());
		render_pass_create_info.pAttachments = attachment_descriptions.data();
		render_pass_create_info.subpassCount = 1;
		render_pass_create_info.pSubpasses = &subpass_description;
		render_pass_create_info.dependencyCount = gsl::narrow_cast<uint>(subpass_dependencies.size());
		render_pass_create_info.pDependencies = subpass_dependencies.data();

		if (vkCreateRenderPass(m_device, &render_pass_create_info, nullptr, &m_render_pass) != VK_SUCCESS) {
			throw std::runtime_error("We could't create a render pass");
		}
	}

	std::cout << "\tRender Pass Created" << std::endl << std::endl;
}

auto Renderer::createDescriptorSetLayout() -> void {

	std::cout << "Creating Descriptor Set Layout" << std::endl;

	auto ubo_layout_binding = VkDescriptorSetLayoutBinding{};
	ubo_layout_binding.binding = 0;
	ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	ubo_layout_binding.descriptorCount = 1;
	ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	ubo_layout_binding.pImmutableSamplers = nullptr; // default value

	auto sampler_layout_binding = VkDescriptorSetLayoutBinding{};
	sampler_layout_binding.binding = 1;
	sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	sampler_layout_binding.descriptorCount = 1;
	sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	sampler_layout_binding.pImmutableSamplers = nullptr; // default value

	auto bindings = std::array<VkDescriptorSetLayoutBinding, 2>{ubo_layout_binding, sampler_layout_binding};

	auto create_info = VkDescriptorSetLayoutCreateInfo{};
	create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	create_info.bindingCount = gsl::narrow_cast<uint>(bindings.size());
	create_info.pBindings = bindings.data();

	if (vkCreateDescriptorSetLayout(m_device, &create_info, nullptr, &m_descriptor_set_layout) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create the descriptor set layout");
	}

	std::cout << "\tDescriptor Set Layout Created" << std::endl << std::endl;

}

auto Renderer::createGraphicsPipeline() -> void {

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

	const auto binding_descriptions = Vertex::getBindingDescription();
	const auto attribute_descriptions = Vertex::getAttributeDescriptions();

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
	rasterizer_create_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer_create_info.depthBiasEnable = VK_FALSE;
	rasterizer_create_info.depthBiasConstantFactor = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor = 0.0f;
	rasterizer_create_info.depthBiasSlopeFactor = 0.0f;


	auto multisampling_create_info = VkPipelineMultisampleStateCreateInfo{};
	multisampling_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling_create_info.sampleShadingEnable = VK_FALSE;
	multisampling_create_info.rasterizationSamples = getSampleBits(config.multisampling_samples);
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

	auto depth_stencil_create_info = VkPipelineDepthStencilStateCreateInfo{};
	depth_stencil_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil_create_info.depthTestEnable = VK_TRUE;
	depth_stencil_create_info.depthWriteEnable = VK_TRUE;
	depth_stencil_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
	depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
	depth_stencil_create_info.minDepthBounds = 0.0f;
	depth_stencil_create_info.maxDepthBounds = 1.0f;
	depth_stencil_create_info.stencilTestEnable = VK_FALSE; //@NOTE: VK_TRUE Only if we have stencil available
	depth_stencil_create_info.front = {};
	depth_stencil_create_info.back = {};

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
	/*
	We set the descriptor set layours over here.
	*/
	pipeline_layout_create_info.setLayoutCount = 1;
	pipeline_layout_create_info.pSetLayouts = &m_descriptor_set_layout;
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
	pipeline_create_info.pDepthStencilState = &depth_stencil_create_info;
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

auto Renderer::createShaderModule(const std::vector<char>& code) const -> VkShaderModule {
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

auto Renderer::createFramebuffers() ->  void {
	std::cout << "Creating Framebuffers " << std::endl;

	m_swap_chain_framebuffers.resize(m_swap_chain_image_views.size());

	for (size_t i = 0; i < m_swap_chain_image_views.size(); ++i) {
		auto attachments = std::vector<VkImageView>{};

		if (config.multisampling_samples == 1) {
			attachments.resize(2);
			attachments[0] = m_swap_chain_image_views[i];
			attachments[1] = m_depth_image_view;
		}
		else {
			attachments.resize(4);
			attachments[0] = m_render_target.view;
			attachments[1] = m_swap_chain_image_views[i];
			attachments[2] = m_depth_target.view;
			attachments[3] = m_depth_image_view;
		}


		auto frame_buffer_create_info = VkFramebufferCreateInfo{};
		frame_buffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
		frame_buffer_create_info.renderPass = m_render_pass;
		frame_buffer_create_info.attachmentCount = gsl::narrow_cast<uint>(attachments.size());
		[[gsl::suppress(bounds.3)]]{
		frame_buffer_create_info.pAttachments = attachments.data();
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

auto Renderer::createGraphicsCommandPool() ->  void {
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

auto Renderer::createTransferCommandPool() ->  void {
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

auto Renderer::createImage(
	uint width,
	uint height,
	VkFormat format,
	VkImageTiling tiling,
	VkImageUsageFlags usage,
	VmaMemoryUsage allocation_usage,
	VmaAllocationCreateFlags allocation_flags,
	AllocatedImage& image,
	VkSharingMode sharing_mode,
	const std::vector<uint>* queue_family_indices,
	short samples) -> void {

	auto create_info = VkImageCreateInfo{};
	{
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.extent.width = gsl::narrow_cast<uint>(width);
		create_info.extent.height = gsl::narrow_cast<uint>(height);
		create_info.extent.depth = 1;
		create_info.mipLevels = 1;
		create_info.arrayLayers = 1;
		create_info.format = format;
		create_info.tiling = tiling;
		create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		create_info.usage = usage;

		create_info.sharingMode = sharing_mode;
		if (sharing_mode == VK_SHARING_MODE_CONCURRENT) {
			if (queue_family_indices == nullptr || queue_family_indices->size() <= 1) {
				throw std::runtime_error("We can't create a shared image without more than one families to share between");
			}
			create_info.queueFamilyIndexCount = gsl::narrow<uint>(queue_family_indices->size());
			create_info.pQueueFamilyIndices = queue_family_indices->data();
		}
		else {
			create_info.queueFamilyIndexCount = 0;
			create_info.pQueueFamilyIndices = nullptr;
		}
		create_info.samples = getSampleBits(samples);
		create_info.flags = 0; // default value
	}

	{
		auto vma_create_info = VmaAllocationCreateInfo{};
		vma_create_info.usage = allocation_usage;
		vma_create_info.flags = allocation_flags;

		if (vmaCreateImage(
			m_vma_allocator,
			&create_info,
			&vma_create_info,
			&image.image,
			&image.allocation,
			&image.allocation_info) != VK_SUCCESS) {
			throw std::runtime_error("We couldn't create a vulkan image to hold the texture image");
		}
	}

}

auto Renderer::destroyImage(AllocatedImage& image) noexcept -> void {
	vmaDestroyImage(m_vma_allocator, image.image, image.allocation);
}

auto Renderer::createDepthResources() -> void {

	const auto depth_format = findDepthFormat();

	createImage(
		m_swap_chain_extent.width,
		m_swap_chain_extent.height,
		depth_format,
		VK_IMAGE_TILING_OPTIMAL,
		VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
		VMA_MEMORY_USAGE_GPU_ONLY,
		0,
		m_depth_image,
		VK_SHARING_MODE_EXCLUSIVE,
		nullptr);

	m_depth_image_view = createImageView(
		m_depth_image.image,
		depth_format,
		VK_IMAGE_ASPECT_DEPTH_BIT);


	changeImageLayout(
		m_depth_image.image,
		depth_format,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

}

auto Renderer::createTextureImage(std::string path)->AllocatedImage {

	std::cout << "Creating Texture Image" << std::endl;

	AllocatedImage image{};

	auto texture_width = 0;
	auto texture_height = 0;
	auto texture_channels = 0;

	auto pixels = stbi_load(
		path.c_str(),
		&texture_width,
		&texture_height,
		&texture_channels,
		STBI_rgb_alpha);

	if (!pixels) {
		throw std::runtime_error("Couldn't load provided texture image");
	}

	[[gsl::suppress(type.4, 6387)]]{
	const auto image_size = VkDeviceSize{ gsl::narrow_cast<VkDeviceSize>(texture_width * texture_height * 4) };


	auto staging_buffer = AllocatedBuffer{};

	/*
	We create and fill the staging buffer for the texture
	*/
	{
		createBuffer(
			image_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
			VMA_MEMORY_USAGE_CPU_TO_GPU,
			VMA_ALLOCATION_CREATE_MAPPED_BIT,
			staging_buffer,
			VK_SHARING_MODE_EXCLUSIVE,
			nullptr);

		memcpy(
			staging_buffer.allocation_info.pMappedData,
			pixels,
			gsl::narrow_cast<size_t>(image_size));
	}


	stbi_image_free(pixels);

	/*
	We create the image that will hold the texture
	*/
	{
		auto queue_family_indices = std::vector<uint>{
			gsl::narrow<uint>(m_queue_family_indices.graphics_family) ,
			gsl::narrow<uint>(m_queue_family_indices.transfer_family)
		};

		std::sort(queue_family_indices.begin(), queue_family_indices.end());
		queue_family_indices.erase(
			std::unique(queue_family_indices.begin(), queue_family_indices.end()),
			queue_family_indices.end());

		createImage(
			texture_width,
			texture_height,
			VK_FORMAT_R8G8B8A8_UNORM,
			VK_IMAGE_TILING_OPTIMAL,
			VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VMA_MEMORY_USAGE_GPU_ONLY,
			0,
			image,
			queue_family_indices.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			&queue_family_indices);
	}

	changeImageLayout(
		image.image,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

	copyBufferToImage(
		staging_buffer.buffer,
		image.image,
		gsl::narrow_cast<uint>(texture_width),
		gsl::narrow_cast<uint>(texture_height));

	changeImageLayout(
		image.image,
		VK_FORMAT_B8G8R8A8_UNORM,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	destroyBuffer(staging_buffer);
	}
	std::cout << "\tTexture Image Created" << std::endl << std::endl;
	return image;
}

auto Renderer::createTextureImageView(AllocatedImage image) -> VkImageView {

	VkImageView image_view;

	std::cout << "Creating Texture Image View " << std::endl;

	image_view = createImageView(
		image.image,
		VK_FORMAT_R8G8B8A8_UNORM,
		VK_IMAGE_ASPECT_COLOR_BIT);

	std::cout << "\tTexture Image View Created" << std::endl << std::endl;

	return image_view;
}


auto Renderer::loadScene(std::string object_path, std::string texture_path) -> void {

	m_scene.indices.clear();
	m_scene.vertices.clear();
	m_scene.m_texture_image = createTextureImage(texture_path);
	m_scene.m_texture_image_view = createTextureImageView(m_scene.m_texture_image);

	tinyobj::attrib_t attributes;
	std::vector<tinyobj::shape_t> shapes;
	std::vector<tinyobj::material_t> materials;
	std::string err;

	if (!tinyobj::LoadObj(&attributes, &shapes, &materials, &err, object_path.c_str())) {
		throw std::runtime_error(err);
	}

	auto unique_vertices = std::unordered_map<Vertex, uint>{};

	for (const auto& shape : shapes) {
		for (const auto& index : shape.mesh.indices) {
			auto vertex = Vertex{};

			vertex.pos = {
				attributes.vertices[3 * index.vertex_index + 0],
				attributes.vertices[3 * index.vertex_index + 1],
				attributes.vertices[3 * index.vertex_index + 2]
			};

			vertex.tex_coord = {
				attributes.texcoords[2 * index.texcoord_index + 0],
				1.0f - attributes.texcoords[2 * index.texcoord_index + 1]
			};

			if (unique_vertices.count(vertex) == 0) {
				unique_vertices[vertex] = gsl::narrow<uint>(m_scene.vertices.size());
				m_scene.vertices.push_back(vertex);
			}

			m_scene.indices.push_back(unique_vertices[vertex]);
		}
	}
}

auto Renderer::createTextureSampler() -> void {

	std::cout << "\tCreating Texture Sampler" << std::endl;

	auto create_info = VkSamplerCreateInfo{};
	{
		create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		/*
		We also could use VK_FILTER_NEAREST for a pixelated look
		*/
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		/*
		Options here are:
			- VK_SAMPLER_ADDRESS_MODE_REPEAT: Just repeat the texture
			- VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT: Repeat and mirroring
			- VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE: Spread color to edges
			- VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE: Spread color to edges and mirror
			- VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER: Just draw the texture and fill the rest with a color

		We pick normal VK_SAMPLER_ADDRESS_MODE_REPEAT since it is the most common
		and usefull when tiling surfaces like floors.
		*/
		create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
		create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

		create_info.anisotropyEnable = VK_TRUE;
		create_info.maxAnisotropy = 16;
		create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

		create_info.unnormalizedCoordinates = VK_FALSE;

		create_info.compareEnable = VK_FALSE;
		create_info.compareOp = VK_COMPARE_OP_ALWAYS;

		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.mipLodBias = 0.0f;
		create_info.minLod = 0.0f;
		create_info.maxLod = 0.0f;
	}

	if (vkCreateSampler(m_device, &create_info, nullptr, &m_texture_sampler) != VK_SUCCESS) {
		throw std::runtime_error("We coulnd't create the texture sampler");
	}

	std::cout << "\tTexture Sampler Created" << std::endl << std::endl;
}

auto Renderer::createBuffer(
	VkDeviceSize size,
	VkBufferUsageFlags usage,
#ifdef VMA_USE_ALLOCATOR
	VmaMemoryUsage allocation_usage,
	VmaAllocationCreateFlags allocation_flags,
#else
	VkMemoryPropertyFlags properties,
#endif
	AllocatedBuffer& allocated_buffer,
	VkSharingMode sharing_mode,
	const std::vector<uint>* queue_family_indices) -> void {

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

#ifdef VMA_USE_ALLOCATOR
	auto allocation_info = VmaAllocationCreateInfo{};
	allocation_info.usage = allocation_usage;
	allocation_info.flags = allocation_flags;

	vmaCreateBuffer(
		m_vma_allocator,
		&buffer_create_info,
		&allocation_info,
		&(allocated_buffer.buffer),
		&(allocated_buffer.allocation),
		&(allocated_buffer.allocation_info));
#else

	if (vkCreateBuffer(m_device, &buffer_create_info, nullptr, &(allocated_buffer.buffer)) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create a buffer");
	}

	auto memory_requirements = VkMemoryRequirements{};
	vkGetBufferMemoryRequirements(m_device, allocated_buffer.buffer, &memory_requirements);

	auto allocate_info = VkMemoryAllocateInfo{};
	allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocate_info.allocationSize = memory_requirements.size;
	allocate_info.memoryTypeIndex = findMemoryType(
		memory_requirements.memoryTypeBits,
		properties);

	if (vkAllocateMemory(m_device, &allocate_info, nullptr, &(allocated_buffer.memory)) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't allocate the required memory for a buffer");
	}

	vkBindBufferMemory(m_device, (allocated_buffer.buffer), (allocated_buffer.memory), 0);
#endif
}

auto Renderer::destroyBuffer(
	AllocatedBuffer& allocated_buffer
) noexcept -> void {

#ifdef VMA_USE_ALLOCATOR
	vmaDestroyBuffer(m_vma_allocator, allocated_buffer.buffer, allocated_buffer.allocation);
#else
	vkDestroyBuffer(m_device, allocated_buffer.buffer, nullptr);
	vkFreeMemory(m_device, allocated_buffer.memory, nullptr);
#endif

}

auto Renderer::createVertexBuffer() -> void {

	std::cout << "Creating Vertex Buffer" << std::endl;


	[[gsl::suppress(type.4)]]{

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

		auto buffer_size = VkDeviceSize{ gsl::narrow_cast<size_t>(sizeof(m_scene.vertices[0]))*m_scene.vertices.size() };

		auto staging_buffer = AllocatedBuffer{};
		createBuffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	#ifndef VMA_USE_ALLOCATOR
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	#else
			VMA_MEMORY_USAGE_CPU_TO_GPU,
			VMA_ALLOCATION_CREATE_MAPPED_BIT,
	#endif
			staging_buffer,
			VK_SHARING_MODE_EXCLUSIVE,
			nullptr);

	#ifdef VMA_USE_ALLOCATOR
		memcpy(staging_buffer.allocation_info.pMappedData, m_scene.vertices.data(), gsl::narrow_cast<size_t>(buffer_size));
	#else
		void *data;
		vkMapMemory(m_device, staging_buffer.memory, 0, buffer_size, 0, &data);
		memcpy(data, vertices.data(), gsl::narrow_cast<size_t>(buffer_size));
		vkUnmapMemory(m_device, staging_buffer.memory);
	#endif

		createBuffer(
			buffer_size,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	#ifdef VMA_USE_ALLOCATOR
			VMA_MEMORY_USAGE_GPU_ONLY,
			0,
	#else
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	#endif
			m_vertex_buffer,
			queue_family_indices.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			&queue_family_indices);

		copyBuffer(staging_buffer.buffer, m_vertex_buffer.buffer, buffer_size);

		destroyBuffer(staging_buffer);
	}
	std::cout << "\tVertex Buffer Created" << std::endl << std::endl;

}

auto Renderer::createIndexBuffer() -> void {

	std::cout << "Creating Index Buffer" << std::endl;

	[[gsl::suppress(type.4)]]{
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

		auto buffer_size = VkDeviceSize{ gsl::narrow_cast<size_t>(sizeof(m_scene.indices[0]))*m_scene.indices.size() };

		auto staging_buffer = AllocatedBuffer{};
		createBuffer(
			buffer_size,
			VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
	#ifndef VMA_USE_ALLOCATOR
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
	#else
			VMA_MEMORY_USAGE_CPU_TO_GPU,
			VMA_ALLOCATION_CREATE_MAPPED_BIT,
	#endif
			staging_buffer,
			VK_SHARING_MODE_EXCLUSIVE,
			nullptr);

	#ifdef VMA_USE_ALLOCATOR
		memcpy(staging_buffer.allocation_info.pMappedData, m_scene.indices.data(), gsl::narrow_cast<size_t>(buffer_size));
	#else
		void *data;
		vkMapMemory(m_device, staging_buffer.memory, 0, buffer_size, 0, &data);
		memcpy(data, indices.data(), gsl::narrow_cast<size_t>(buffer_size));
		vkUnmapMemory(m_device, staging_buffer.memory);
	#endif

		createBuffer(
			buffer_size,
			VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
	#ifdef VMA_USE_ALLOCATOR
			VMA_MEMORY_USAGE_GPU_ONLY,
			0,
	#else
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
	#endif
			m_index_buffer,
			queue_family_indices.size() > 1 ? VK_SHARING_MODE_CONCURRENT : VK_SHARING_MODE_EXCLUSIVE,
			&queue_family_indices);

		copyBuffer(staging_buffer.buffer, m_index_buffer.buffer, buffer_size);

		destroyBuffer(staging_buffer);
	}

	std::cout << "\tIndex Buffer Created" << std::endl << std::endl;
}

auto Renderer::createUniformBuffer() -> void {

	std::cout << "Creating Uniform Buffer" << std::endl;

	[[gsl::suppress(type.4)]]{
	const auto buffer_size = VkDeviceSize{ gsl::narrow_cast<size_t>(sizeof(UniformBufferObject)) };

	createBuffer(
		buffer_size,
		VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
#ifndef VMA_USE_ALLOCATOR
		VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
#else
		VMA_MEMORY_USAGE_CPU_TO_GPU,
		VMA_ALLOCATION_CREATE_MAPPED_BIT,
#endif
		m_uniform_buffer,
		VK_SHARING_MODE_EXCLUSIVE,
		nullptr);
}

	std::cout << "\tUniform Buffer Created" << std::endl << std::endl;
}

auto Renderer::createDescriptorPool() -> void {

	std::cout << "Creating Descriptor Pool" << std::endl;


	auto pool_sizes = std::array<VkDescriptorPoolSize, 2>{};
	/*
	@NOTE: Change this to DYNAMIC if necessary
	*/
	pool_sizes.at(0).type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	pool_sizes.at(0).descriptorCount = 1;
	pool_sizes.at(1).type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	pool_sizes.at(1).descriptorCount = 1;


	auto create_info = VkDescriptorPoolCreateInfo{};
	create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	create_info.poolSizeCount = gsl::narrow_cast<uint>(pool_sizes.size());
	create_info.pPoolSizes = pool_sizes.data();
	create_info.maxSets = 1;
	/*
	@NOTE: Use the VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT if we
	want to free individual descriptor sets
	*/
	create_info.flags = 0;

	if (vkCreateDescriptorPool(m_device, &create_info, nullptr, &m_descriptor_pool) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create the descriptor pool");
	}

	std::cout << "\tDescriptor Pool Created" << std::endl << std::endl;
}

auto Renderer::createDescriptorSet() -> void {

	std::cout << "Creating Descriptor Set" << std::endl;

	VkDescriptorSetLayout layouts[] = { m_descriptor_set_layout };
	auto alloc_info = VkDescriptorSetAllocateInfo{};
	alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	alloc_info.descriptorPool = m_descriptor_pool;
	alloc_info.descriptorSetCount = 1;
	{
		[[gsl::suppress(bounds.3)]]{
		alloc_info.pSetLayouts = layouts;
		}
	}

	if (vkAllocateDescriptorSets(m_device, &alloc_info, &m_descriptor_set) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't allocate the descriptor set");
	}

	auto buffer_info = VkDescriptorBufferInfo{};
	buffer_info.buffer = m_uniform_buffer.buffer;
	buffer_info.offset = 0;
	buffer_info.range = sizeof(UniformBufferObject);

	auto image_info = VkDescriptorImageInfo{};
	image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
	image_info.imageView = m_scene.m_texture_image_view;
	image_info.sampler = m_texture_sampler;

	auto descriptor_writes = std::array<VkWriteDescriptorSet, 2>{};
	{
		descriptor_writes.at(0).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes.at(0).dstSet = m_descriptor_set;
		descriptor_writes.at(0).dstBinding = 0;
		descriptor_writes.at(0).dstArrayElement = 0;
		descriptor_writes.at(0).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		descriptor_writes.at(0).descriptorCount = 1;
		descriptor_writes.at(0).pBufferInfo = &buffer_info;
		descriptor_writes.at(0).pImageInfo = nullptr; // default value
		descriptor_writes.at(0).pTexelBufferView = nullptr; // default value

		descriptor_writes.at(1).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
		descriptor_writes.at(1).dstSet = m_descriptor_set;
		descriptor_writes.at(1).dstBinding = 1;
		descriptor_writes.at(1).dstArrayElement = 0;
		descriptor_writes.at(1).descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		descriptor_writes.at(1).descriptorCount = 1;
		descriptor_writes.at(1).pBufferInfo = nullptr; // default value
		descriptor_writes.at(1).pImageInfo = &image_info;
		descriptor_writes.at(1).pTexelBufferView = nullptr; // default value
	}


	vkUpdateDescriptorSets(
		m_device,
		gsl::narrow_cast<uint>(descriptor_writes.size()),
		descriptor_writes.data(),
		0,
		nullptr);

	std::cout << "\tDescriptor Set Created" << std::endl << std::endl;
}

auto Renderer::findDepthFormat() -> VkFormat {

	if (m_depth_format == VK_FORMAT_UNDEFINED) {
		m_depth_format = findSupportedFormat(
			{
				VK_FORMAT_D32_SFLOAT_S8_UINT,
				VK_FORMAT_D24_UNORM_S8_UINT
				/* Because we want stencil we remove[,VK_FORMAT_D32_SFLOAT]*/ },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
	}
	return m_depth_format;
}

auto  Renderer::hasStencilComponent(VkFormat format) const noexcept -> bool {
	return (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT);
}

auto Renderer::findSupportedFormat(
	const std::vector<VkFormat>& candidates,
	VkImageTiling tiling,
	VkFormatFeatureFlags features)->VkFormat {

	for (auto& format : candidates) {
		auto properties = VkFormatProperties{};
		vkGetPhysicalDeviceFormatProperties(m_physical_device, format, &properties);
		if (tiling == VK_IMAGE_TILING_LINEAR && ((properties.linearTilingFeatures & features) == features)) {
			return format;
		}
		else if (tiling == VK_IMAGE_TILING_OPTIMAL && ((properties.optimalTilingFeatures & features) == features)) {
			return format;
		}
	}

	throw std::runtime_error("We couldn't find an appropriate format");
}

auto Renderer::findMemoryType(uint type_bits, VkMemoryPropertyFlags properties, VkBool32 *found)->uint {

	auto memory_properties = VkPhysicalDeviceMemoryProperties{};
	vkGetPhysicalDeviceMemoryProperties(m_physical_device, &memory_properties);


	for (uint32_t i = 0; i < memory_properties.memoryTypeCount; i++)
	{
		if ((type_bits & 1) == 1)
			[[gsl::suppress(bounds.2)]]
		{
			if ((memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				if (found)
				{
					*found = true;
				}
				return i;
			}
		}
		type_bits >>= 1;
	}

	if (found)
	{
		*found = false;
		return 0;
	}
	else
	{
		throw std::runtime_error("We couldn't find an appropriate memory type");
	}
}

auto Renderer::copyBuffer(VkBuffer src, VkBuffer dst, VkDeviceSize size) noexcept -> void {


	auto command_buffer = beginSingleTimeCommands(CommandType::transfer);

	auto copy_region = VkBufferCopy{};
	copy_region.srcOffset = 0;
	copy_region.dstOffset = 0;
	copy_region.size = size;
	vkCmdCopyBuffer(command_buffer.buffer, src, dst, 1, &copy_region);

	endSingleTimeCommands(command_buffer);

}

auto Renderer::createCommandBuffers() ->  void {
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

	m_command_buffer_submitted.resize(m_command_buffers.size());
	std::fill(m_command_buffer_submitted.begin(), m_command_buffer_submitted.end(), false);

	std::cout << "\tCommand Buffers Created" << std::endl << std::endl;
}

auto Renderer::recordCommandBuffers() -> void {
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
		/*
		@NOTE: This should be using m_current_swapchain_buffer if we are recording
		before each frame.

		@see m_current_swapchain_buffer
		*/

		std::array<VkClearValue, 3> clear_values = { config::clear_color , config::clear_color , config::clear_color };

		if (config.multisampling_samples == 1) {

			clear_values[0].color = { 0.0f,0.0f,0.0f,0.0f };
			clear_values[1].depthStencil = { 1.0f, 0 };

			render_info.framebuffer = m_swap_chain_framebuffers[i];
			render_info.renderArea.offset = { 0, 0 };
			render_info.renderArea.extent = m_swap_chain_extent;
			render_info.clearValueCount = 2;
			render_info.pClearValues = clear_values.data();
		}
		else {

			clear_values[0].color = { 0.0f,0.0f,0.0f,1.0f };
			clear_values[1].color = { 0.0f,0.0f,0.0f,1.0f };
			clear_values[2].depthStencil = { 1.0f, 0 };
			render_info.framebuffer = m_swap_chain_framebuffers[i];
			render_info.renderArea.offset = { 0, 0 };
			render_info.renderArea.extent = m_swap_chain_extent;
			render_info.clearValueCount = 3;
			render_info.pClearValues = clear_values.data();
		}

		vkCmdBeginRenderPass(m_command_buffers[i], &render_info, VK_SUBPASS_CONTENTS_INLINE);

		{
			vkCmdBindPipeline(m_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline);

			const VkBuffer vertex_buffers[] = { m_vertex_buffer.buffer };
			const VkDeviceSize offsets[] = { 0 };

			[[gsl::suppress(bounds.3)]]{
			vkCmdBindVertexBuffers(m_command_buffers[i], 0, 1, vertex_buffers, offsets);
			}

			vkCmdBindIndexBuffer(m_command_buffers[i], m_index_buffer.buffer, 0, VK_INDEX_TYPE_UINT32);

			vkCmdBindDescriptorSets(
				m_command_buffers[i],
				VK_PIPELINE_BIND_POINT_GRAPHICS,
				m_pipeline_layout,
				0,
				1,
				&m_descriptor_set,
				0,
				nullptr);

			vkCmdDrawIndexed(m_command_buffers[i], gsl::narrow<uint>(m_scene.indices.size()), 1, 0, 0, 0);
		}

		vkCmdEndRenderPass(m_command_buffers[i]);

		if (vkEndCommandBuffer(m_command_buffers[i]) != VK_SUCCESS) {
			throw std::runtime_error("We couldn't record a command buffer");
		}

	}

	std::cout << "\tCommand Buffers Recorded" << std::endl << std::endl;
}


auto Renderer::createSemaphoresAndFences() -> void {

	std::cout << "Creating Semaphores And Fences" << std::endl;

	m_image_available_semaphores.resize(m_swap_chain_images.size());

	for (auto i = 0; i < m_swap_chain_images.size(); ++i) {

		auto semaphore_create_info = VkSemaphoreCreateInfo{};
		memset(&semaphore_create_info, 0, sizeof(semaphore_create_info));
		semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(m_device, &semaphore_create_info, nullptr, &m_image_available_semaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("We couldn't create a semaphore to check when the image is available");
		}
	}

	m_render_finished_semaphores.resize(m_command_buffers.size());
	m_command_buffer_fences.resize(m_command_buffers.size());

	auto fence_create_info = VkFenceCreateInfo{};
	memset(&fence_create_info, 0, sizeof(fence_create_info));
	fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;

	for (auto i = 0; i < m_command_buffers.size(); ++i) {

		auto semaphore_create_info = VkSemaphoreCreateInfo{};
		memset(&semaphore_create_info, 0, sizeof(semaphore_create_info));
		semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		if (vkCreateSemaphore(m_device, &semaphore_create_info, nullptr, &m_render_finished_semaphores[i]) != VK_SUCCESS) {
			throw std::runtime_error("We couldn't create semaphore to check when rendering has been finished");
		}

		if (vkCreateFence(m_device, &fence_create_info, nullptr, &m_command_buffer_fences[i]) != VK_SUCCESS) {
			throw std::runtime_error("We couldn't create fence to check when rendering has been finished");
		}

	}

	std::cout << "\tSemaphores And Fences Created" << std::endl << std::endl;

}


auto Renderer::updateRotateTestUniformBuffer() ->void {

	static auto start_time = std::chrono::high_resolution_clock::now();

	const auto current_time = std::chrono::high_resolution_clock::now();

	auto time = std::chrono::duration
		<float, std::chrono::seconds::period>
		(current_time - start_time).count();

	auto ubo = UniformBufferObject{};

	ubo.model = glm::rotate(glm::mat4(1.0f), time* glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.view = glm::lookAt(
		glm::vec3(1.0f, 0.2f, 1.2f),
		glm::vec3(0.0f, 0.0f, 0.5f),
		glm::vec3(0.0f, 0.0f, 1.0f));
	ubo.proj = glm::perspective(
		glm::radians(45.0f),
		m_swap_chain_extent.width / gsl::narrow_cast<float>(m_swap_chain_extent.height),
		0.1f,
		10.0f);
	ubo.proj[1][1] *= -1; // We compensate for the inverted Y axis in GLM (meant for OpenGL)

	

#ifdef VMA_USE_ALLOCATOR
	memcpy(m_uniform_buffer.allocation_info.pMappedData, &ubo, sizeof(ubo));
#else
	void *data = nullptr;
	vkMapMemory(m_device, m_uniform_buffer.allocation_info.deviceMemory, 0, sizeof(ubo), 0, &data);
	memcpy(data, &ubo, sizeof(ubo));
	vkUnmapMemory(m_device, m_uniform_buffer.allocation_info.deviceMemory);
#endif

}

auto Renderer::beginFrame() -> void {


	/*
	We wait for the fence that indicates that we can use the current
	command buffer
	*/

	{
		if (m_command_buffer_submitted[m_current_command_buffer]) {

			if (vkWaitForFences(
				m_device,
				1,
				&m_command_buffer_fences[m_current_command_buffer],
				VK_TRUE,
				std::numeric_limits<uint64_t>::max())
				!= VK_SUCCESS) {
				throw std::runtime_error("We couldn't wait for the fence involving the current command buffer");
			}
		}

		if (vkResetFences(m_device, 1, &m_command_buffer_fences[m_current_command_buffer])) {
			throw std::runtime_error("We couldn't reset the fence involving the current command buffer");
		}
	}


	/*
	@TODO: Beginning of the recording of the current Command Buffer
	using the m_current_command_buffer index and maybe set up some data of the
	next "render pass begin info".

	https://github.com/Novum/vkQuake/blob/master/Quake/gl_vidsdl.c @ line 1787, 1823
	https://github.com/ocornut/imgui/blob/master/examples/vulkan_example/main.cpp @ line 507
	*/


	/*
	@TODO: Beginning of Render Pass using m_current_swapchain_buffer

	https://github.com/Novum/vkQuake/blob/master/Quake/gl_screen.c @ line 985(begin render pass), line 987 (acquire next image)
	https://github.com/Novum/vkQuake/blob/master/Quake/view.c @ line 888
	https://github.com/Novum/vkQuake/blob/master/Quake/gl_rmain.c @ line 554, 524
	https://github.com/ocornut/imgui/blob/master/examples/vulkan_example/main.cpp @ line 507
	*/


	/*
	We acquire the intex to the image we will render next
	after we begin recording of the command buffer and start the current render pass.
	*/
	{
		const auto result = vkAcquireNextImageKHR(
			m_device,
			m_swap_chain,
			/*
			This is the timeout in ns for an image to become available
			*/
			std::numeric_limits<uint64_t>::max(),
			m_image_available_semaphores[m_current_command_buffer],
			VK_NULL_HANDLE,
			&m_current_swapchain_buffer);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) {
			recreateSwapChain();
			return;
		}
		else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
			throw std::runtime_error("We couldn't acquire an image to render to");
		}
	}

}

auto Renderer::endFrame() -> void {

	/*
	@TODO: Ending of Render Pass and Command Buffer (in that order)

	https://github.com/ocornut/imgui/blob/master/examples/vulkan_example/main.cpp @ line 543
	*/


	/*
	Semaphore that indicates that rendering is done
	*/
	VkSemaphore signal_semaphores[] = { m_render_finished_semaphores[m_current_command_buffer] };

	/*
	We submit the current command buffer to the graphics queue and reset the fences.
	*/
	[[gsl::suppress(bounds.3)]]
	{
		auto submit_info = VkSubmitInfo{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

		const VkSemaphore wait_semaphores[] = { m_image_available_semaphores[m_current_command_buffer] };
		const VkPipelineStageFlags wait_stages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &m_command_buffers[m_current_command_buffer];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		if (vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_command_buffer_fences[m_current_command_buffer]) != VK_SUCCESS) {
			throw std::runtime_error("We couldn't submit our command buffer");
		}
	}

		/*
		We express that we have submitted the current command buffer and update
		the index of the current command buffer we are using.
		*/
	{
		m_command_buffer_submitted[m_current_command_buffer] = true;
		m_current_command_buffer = (m_current_command_buffer + 1) % m_command_buffers.size();
	}

	/*
	We finally present the image
	*/
	[[gsl::suppress(bounds.3)]]
	{
		auto present_info = VkPresentInfoKHR{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;
		VkSwapchainKHR swap_chains[] = { m_swap_chain };
		present_info.swapchainCount = 1;
		present_info.pSwapchains = swap_chains;
		present_info.pImageIndices = &m_current_swapchain_buffer;
		present_info.pResults = nullptr;


		const auto result = vkQueuePresentKHR(m_present_queue, &present_info);

		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
			recreateSwapChain();
		}
		else if (result != VK_SUCCESS) {
			throw std::runtime_error("We couldn't submit the presentation info to the queue");
		}
	}
}

auto Renderer::onWindowsResized(GLFWwindow * window, int width, int height) -> void {

	Renderer* render_manager = nullptr;

	[[gsl::suppress(type.1)]]{
		render_manager = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
	}

		if (render_manager != nullptr)
			render_manager->recreateSwapChain();

	std::cout << " - Window resized to (" << width << ", " << height << ")" << std::endl;
}

auto Renderer::beginSingleTimeCommands(CommandType command_type) noexcept ->WrappedCommandBuffer {
	auto allocate_info = VkCommandBufferAllocateInfo{};
	allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

	switch (command_type) {
	case CommandType::graphics: {
		allocate_info.commandPool = m_graphics_command_pool;
		break;
	}
	case CommandType::transfer: {
		allocate_info.commandPool = m_transfer_command_pool;
		break;
	}
	}

	allocate_info.commandBufferCount = 1;

	auto command_buffer = WrappedCommandBuffer{};
	command_buffer.type = command_type;
	vkAllocateCommandBuffers(m_device, &allocate_info, &command_buffer.buffer);

	auto begin_info = VkCommandBufferBeginInfo{};
	begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	vkBeginCommandBuffer(command_buffer.buffer, &begin_info);
	command_buffer.recording = true;

	return command_buffer;
}

auto Renderer::endSingleTimeCommands(WrappedCommandBuffer& command_buffer) noexcept ->void {

	vkEndCommandBuffer(command_buffer.buffer);
	command_buffer.recording = false;

	auto submit_info = VkSubmitInfo{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer.buffer;

	auto command_pool = VkCommandPool{};
	auto queue = VkQueue{};

	switch (command_buffer.type) {
	case CommandType::graphics: {
		command_pool = m_graphics_command_pool;
		queue = m_graphics_queue;
		break;
	}
	case CommandType::transfer: {
		command_pool = m_transfer_command_pool;
		queue = m_transfer_queue;
		break;
	}
	}

	vkQueueSubmit(queue, 1, &submit_info, VK_NULL_HANDLE);
	vkQueueWaitIdle(queue);

	vkFreeCommandBuffers(m_device, command_pool, 1, &command_buffer.buffer);
}

auto Renderer::changeImageLayout(
	VkImage& image,
	VkFormat format,
	VkImageLayout old_layout,
	VkImageLayout new_layout)->void {

	auto command_buffer = beginSingleTimeCommands();
	{

		auto barrier = VkImageMemoryBarrier{};
		auto source_stage = VkPipelineStageFlags{};
		auto destination_stage = VkPipelineStageFlags{};
		{
			barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			barrier.oldLayout = old_layout;
			barrier.newLayout = new_layout;
			barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			barrier.image = image;
			barrier.subresourceRange.baseMipLevel = 0;
			barrier.subresourceRange.levelCount = 1;
			barrier.subresourceRange.baseArrayLayer = 0;
			barrier.subresourceRange.layerCount = 1;

			if (new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
				if (hasStencilComponent(format)) {
					barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
				}
			}
			else {
				barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			}

			if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

				source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			}
			else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
				barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
				barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

				source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
				destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			}
			else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
				barrier.srcAccessMask = 0;
				barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

				source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
				destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
			}
			else {
				throw std::invalid_argument("Unsupported image layout transition");
			}



		}

		vkCmdPipelineBarrier(
			command_buffer.buffer,
			source_stage, destination_stage,
			0,
			0, nullptr,
			0, nullptr,
			1, &barrier
		);

	}
	endSingleTimeCommands(command_buffer);
}

auto Renderer::copyBufferToImage(
	VkBuffer buffer,
	VkImage image,
	uint width,
	uint heigth) noexcept -> void {

	auto command_buffer = beginSingleTimeCommands();
	{

		auto region = VkBufferImageCopy{};
		{
			region.bufferOffset = 0;
			region.bufferRowLength = 0;
			region.bufferImageHeight = 0;

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = 0;
			region.imageSubresource.layerCount = 1;

			region.imageOffset = { 0, 0, 0 };
			region.imageExtent = { width, heigth, 1 };
		}

		vkCmdCopyBufferToImage(
			command_buffer.buffer,
			buffer,
			image,
			VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			1,
			&region);

	}
	endSingleTimeCommands(command_buffer);
}

auto Renderer::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspect_flags)->VkImageView {

	auto create_info = VkImageViewCreateInfo{};
	create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	create_info.flags;
	create_info.image = image;
	create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	create_info.format = format;
	create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
	create_info.subresourceRange.aspectMask = aspect_flags;
	create_info.subresourceRange.baseArrayLayer = 0;
	create_info.subresourceRange.layerCount = 1;
	create_info.subresourceRange.baseMipLevel = 0;
	create_info.subresourceRange.levelCount = 1;

	auto image_view = VkImageView{};

	if (vkCreateImageView(m_device, &create_info, nullptr, &image_view) != VK_SUCCESS) {
		throw std::runtime_error("Couldn't create an image view for the provided image");
	}

	return image_view;
}

auto Renderer::getSampleBits(short samples)->VkSampleCountFlagBits {

	switch (samples) {
	case 1: return VK_SAMPLE_COUNT_1_BIT;
	case 2: return VK_SAMPLE_COUNT_2_BIT;
	case 4: return VK_SAMPLE_COUNT_4_BIT;
	case 8: return VK_SAMPLE_COUNT_8_BIT;
	default:
		throw std::invalid_argument("Amount of samples supported are 1, 2, 4 or 8");
	}

}

auto Renderer::createMultisampleRenderTarget(
	uint width,
	uint height,
	VkFormat format,
	VkImageUsageFlags usage,
	VkImageAspectFlags aspect_mask)->WrappedRenderTarget {

	auto image_create_info = VkImageCreateInfo{};
	image_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_create_info.imageType = VK_IMAGE_TYPE_2D;
	image_create_info.format = format;
	image_create_info.extent.width = width;
	image_create_info.extent.height = height;
	image_create_info.extent.depth = 1;
	image_create_info.mipLevels = 1;
	image_create_info.arrayLayers = 1;
	image_create_info.samples = getSampleBits(config.multisampling_samples);
	image_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	image_create_info.tiling = VK_IMAGE_TILING_OPTIMAL;
	image_create_info.usage = usage;
	image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	auto image = VkImage{};
	auto memory = VkDeviceMemory{};
	if (vkCreateImage(m_device, &image_create_info, nullptr, &image) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create an image for the render target");
	}

	auto memory_requirements = VkMemoryRequirements{};
	vkGetImageMemoryRequirements(m_device, image, &memory_requirements);

	auto alloc = VkMemoryAllocateInfo{};
	alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	alloc.allocationSize = memory_requirements.size;


	/*
	@NOTE: We usually can't use LAZILY allocated memory here because
	lazily-allocated memory and transient attachments is not
	a thing on desktop.
	*/
	VkBool32 memory_type_present;
	alloc.memoryTypeIndex = findMemoryType(
		memory_requirements.memoryTypeBits,
		VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT,
		&memory_type_present);
	if (!memory_type_present)
	{
		/*
		So if LAZILY allocated memory is not available we fallback
		to normal device local memory.
		*/
		std::cout << "\tLazy memory not supported on the system, falling back to device local memory" << std::endl;
		alloc.memoryTypeIndex = findMemoryType(
			memory_requirements.memoryTypeBits,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
			&memory_type_present);

		if (!memory_type_present) {
			throw std::runtime_error("We can't find an appropriate memory type for the render target");
		}
	}

	if (vkAllocateMemory(m_device, &alloc, nullptr, &memory) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't allocate memory for an image for the render target");
	}

	vkBindImageMemory(m_device, image, memory, 0);

	auto view_create_info = VkImageViewCreateInfo{};
	view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view_create_info.image = image;
	view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view_create_info.format = format;
	view_create_info.components.r = VK_COMPONENT_SWIZZLE_R;
	view_create_info.components.g = VK_COMPONENT_SWIZZLE_G;
	view_create_info.components.b = VK_COMPONENT_SWIZZLE_B;
	view_create_info.components.a = VK_COMPONENT_SWIZZLE_A;
	view_create_info.subresourceRange.aspectMask = aspect_mask;
	view_create_info.subresourceRange.levelCount = 1;
	view_create_info.subresourceRange.layerCount = 1;

	auto view = VkImageView{};

	if (vkCreateImageView(m_device, &view_create_info, nullptr, &view) != VK_SUCCESS) {
		throw std::runtime_error("We couldn't create an image view for the image for the render target");
	}

	return WrappedRenderTarget{ image, memory, view, width, height, true };;
}
