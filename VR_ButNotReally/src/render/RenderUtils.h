#pragma once
#include <iostream>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

struct GLFWWindowDestroyer {
	auto operator()(GLFWwindow* ptr) noexcept -> void;
};

/*
Functions to extend VkPhysicalDevice
*/
auto operator<<(std::ostream & stream, const VkPhysicalDevice& device)->std::ostream&;
[[gsl::suppress(bounds.3)]] auto getPhysicalDeviceName(const VkPhysicalDevice& device)->std::string;

struct QueueFamilyIndices {
	int graphics_family = -1;

	auto isComplete() noexcept {
		return graphics_family >= 0;
	}
};


auto getVulkanQueueFlagNames(const int& flags)->std::string;
