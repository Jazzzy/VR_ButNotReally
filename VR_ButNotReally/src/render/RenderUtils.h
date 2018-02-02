#pragma once
#include <iostream>
#include <vulkan/vulkan.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>

/**
Functor to destroy the GLFWwindow when its unique
pointer goes out of scope.
*/
struct GLFWWindowDestroyer {
	auto operator()(GLFWwindow* ptr) noexcept -> void;
};

/**
Stores the queue indices for the graphics and presentation
queues in a vulkan physical device.
*/
struct QueueFamilyIndices {
	int graphics_family = -1;
	int present_family = -1;

	/**
	Returns wether all the data members have been filled with 
	valid queue indices.
	
	return true if all the indices have been correctly filled, false otherwise
	*/
	auto isComplete() noexcept {
		return	graphics_family >= 0 &&
			present_family >= 0;
	}
};

/**
Encapsulates various details about the swap chain
support for a particular physical device
*/
struct SwapChainSupportDetails {
	VkSurfaceCapabilitiesKHR capabilities{};
	std::vector<VkSurfaceFormatKHR> formats{};
	std::vector<VkPresentModeKHR> present_modes{};
};

/**
Defining the operator<< to print information about a physical device. 
*/
auto operator<<(std::ostream & stream, const VkPhysicalDevice& device)->std::ostream&;

/**
Defining the operator<< to print information about a present mode.
*/
auto operator<<(std::ostream & stream, const VkPresentModeKHR& present_mode)->std::ostream&;

/**
Generates a simple name for a physical device.

@param The physical device to get a name for
@return A string with the simple name for the physical device
*/
[[gsl::suppress(bounds.3)]] auto getPhysicalDeviceName(const VkPhysicalDevice& device)->std::string;

/**
Generates a string with the names of the vulkan queue flags provided.

@param An int containing the flag bits set to 1
@return A string with the names of the flags
*/
auto getVulkanQueueFlagNames(const int& flags)->std::string;

