#pragma once
#include <exception>
#include <iostream>
#include <cstdlib>
#include <functional>
#include <memory>
#include <vector>

#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>



struct GLFWWindowDestroyer {
	auto operator()(GLFWwindow* ptr) noexcept -> void;
};

class RenderManager
{
public:
	using WindowPtr = std::unique_ptr<GLFWwindow, GLFWWindowDestroyer>;
	using uint = uint32_t;

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

	auto printInstanceExtensions(const std::vector<VkExtensionProperties> extensions) const -> void;

	[[gsl::suppress(con.3)]] auto checkInstanceExtensionsNamesAvailable(const std::vector<const char*> required_extensions, const std::vector<VkExtensionProperties> available_extensions) const -> bool;

	auto checkValidationLayerSupport() const noexcept -> bool;

	auto getRequiredExtensions() const noexcept->std::vector<const char*>;

#pragma warning( push )
#pragma warning( disable : 4229)
	auto static debugReportCallback(
		VkDebugReportFlagsEXT                       flags,
		VkDebugReportObjectTypeEXT                  object_type,
		uint64_t                                    object,
		size_t                                      location,
		int32_t                                     msg_code,
		const char*                                 layer_prefix,
		const char*                                 msg,
		void*                                       user_data
	)->VKAPI_ATTR VkBool32 VKAPI_CALL;
#pragma warning( pop )

	auto setupDebugCallback() -> void;

	auto createDebugReportCallbackEXT(
		VkInstance instance,
		const VkDebugReportCallbackCreateInfoEXT * create_info,
		const VkAllocationCallbacks* allocator,
		VkDebugReportCallbackEXT* callback
	)->VkResult;

	auto destroyDebugReportCallbackEXT(
		VkInstance instance,
		VkDebugReportCallbackEXT callback,
		const VkAllocationCallbacks* allocator
	) -> void;


	/* --- Members --- */

	WindowPtr m_window;

	VkInstance m_instance;

	VkDebugReportCallbackEXT m_debug_callback;

};

