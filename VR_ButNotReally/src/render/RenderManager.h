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

	auto printInstanceExtensions(const std::vector<VkExtensionProperties> extensions) const -> void;

	[[gsl::suppress(con.3)]] auto checkInstanceExtensionsNamesAvailable(const char** const required_names, const uint name_count, const std::vector<VkExtensionProperties> available_extensions) const -> bool;

	auto checkValidationLayerSupport() const noexcept -> bool;

	auto getRequiredExtensions() const noexcept -> std::vector<const char*> ;

	/*Members*/
	WindowPtr m_window;

	VkInstance m_instance;

};

