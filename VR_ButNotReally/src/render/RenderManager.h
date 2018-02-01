#pragma once
#include <exception>
#include <iostream>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>
#include <gsl/gsl>


#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>


#include "../utils/Utils.h"
#include "./RenderUtils.h"


class RenderManager
{
private:
	using WindowPtr = std::unique_ptr<GLFWwindow, GLFWWindowDestroyer>;
	using uint = uint32_t;

public:
	[[gsl::suppress(26439)]] RenderManager();
	RenderManager(const RenderManager&) = delete;
	RenderManager& operator=(const RenderManager&) = delete;
	RenderManager(RenderManager&&) = delete;
	RenderManager& operator=(RenderManager&&) = delete;
	~RenderManager();


	auto update() noexcept -> void;

	auto shouldClose() const noexcept -> bool;

private:

	auto initWindow() noexcept -> void;

	auto initVulkan() -> void;

	auto loopIteration() noexcept -> void;

	auto cleanup() noexcept -> void;

	auto createInstance()->VkInstance;

	auto printInstanceExtensions(const std::vector<VkExtensionProperties>& extensions) const -> void;

	[[gsl::suppress(bounds.3)]] auto checkInstanceExtensionsNamesAvailable(const std::vector<const char*>& required_extensions, const std::vector<VkExtensionProperties>& available_extensions) const -> bool;

	[[gsl::suppress(bounds.3)]] auto checkValidationLayerSupport() const noexcept -> bool;

	auto getRequiredExtensions() const noexcept->std::vector<const char*>;

#pragma warning( push )
#pragma warning( disable : 4229)
	auto static VKAPI_ATTR VKAPI_CALL debugReportCallback(
		VkDebugReportFlagsEXT                       flags,
		VkDebugReportObjectTypeEXT                  object_type,
		uint64_t                                    object,
		size_t                                      location,
		int32_t                                     msg_code,
		const char*                                 layer_prefix,
		const char*                                 msg,
		void*                                       user_data
	)->VkBool32;
#pragma warning( pop )

	auto setupDebugCallback() -> void;

	auto createSurface() -> void;

	auto createDebugReportCallbackEXT(
		const VkInstance& instance,
		const VkDebugReportCallbackCreateInfoEXT * create_info,
		const VkAllocationCallbacks* allocator,
		VkDebugReportCallbackEXT* callback
	) noexcept->VkResult;

	auto destroyDebugReportCallbackEXT(
		const VkInstance& instance,
		const VkDebugReportCallbackEXT& callback,
		const VkAllocationCallbacks* allocator
	) noexcept -> void;

	auto pickPhysicalDevice() -> void;

	auto physicalDeviceSuitability(const VkPhysicalDevice& device) const noexcept->std::tuple<bool, int>;

	auto findQueueFamilies(const VkPhysicalDevice& physical_device, PrintOptions print_options) const->QueueFamilyIndices;

	auto createLogicalDevice() -> void;

	[[gsl::suppress(bounds.3)]] auto checkDeviceExtensionSupport(const VkPhysicalDevice& device) const -> bool;

	auto querySwapChainSupport(const VkPhysicalDevice& device) const->SwapChainSupportDetails;

	auto pickSurfaceChainFormat(const std::vector<VkSurfaceFormatKHR>& available_formats) const->VkSurfaceFormatKHR;

	auto pickSurfacePresentMode(const std::vector<VkPresentModeKHR>& available_modes)  const noexcept->VkPresentModeKHR;

	auto pickSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)  const noexcept->VkExtent2D;

	auto createSwapChain() -> void;

	/* --- Members --- */

	WindowPtr m_window{};

	VkInstance m_instance{};

	VkDebugReportCallbackEXT m_debug_callback{};

	VkSurfaceKHR m_surface{};

	QueueFamilyIndices m_queue_family_indices{};

	VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;

	// This one is the main logical device 
	VkDevice m_device{};

	VkQueue m_graphics_queue{};

	VkQueue m_present_queue{};

	VkSwapchainKHR m_swap_chain{};

	std::vector<VkImage> m_swap_chain_images{};

	VkFormat m_swap_chain_image_format{};

	VkExtent2D m_swap_chain_extent{};

};

