#pragma once
#include <iostream>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

struct GLFWWindowDestroyer {
	auto operator()(GLFWwindow* ptr) noexcept -> void;
};

struct QueueFamilyIndices {
	int graphics_family = -1;
	int present_family = -1;

	auto isComplete() noexcept {
		return	graphics_family >= 0 &&
			present_family >= 0;
	}
};


struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats{};
	std::vector<VkPresentModeKHR> present_modes{};
};

/*
Functions to extend VkPhysicalDevice
*/
auto operator<<(std::ostream & stream, const VkPhysicalDevice& device)->std::ostream&;
auto operator<<(std::ostream & stream, const VkPresentModeKHR& present_mode)->std::ostream&;



[[gsl::suppress(bounds.3)]] auto getPhysicalDeviceName(const VkPhysicalDevice& device)->std::string;


auto getVulkanQueueFlagNames(const int& flags)->std::string;

