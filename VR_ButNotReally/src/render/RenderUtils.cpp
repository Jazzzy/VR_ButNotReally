#include "RenderUtils.h"
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>


auto GLFWWindowDestroyer::operator()(GLFWwindow* ptr) noexcept -> void {
	glfwDestroyWindow(ptr);
}

auto getPhysicalDeviceName(const VkPhysicalDevice& device) -> std::string {
	auto properties = VkPhysicalDeviceProperties{};
	vkGetPhysicalDeviceProperties(device, &properties);
	return std::string(properties.deviceName);
}

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


auto operator<<(std::ostream & stream, const VkPresentModeKHR& present_mode)->std::ostream& {

	/*
	VK_PRESENT_MODE_IMMEDIATE_KHR = 0,
		VK_PRESENT_MODE_MAILBOX_KHR = 1,
		VK_PRESENT_MODE_FIFO_KHR = 2,
		VK_PRESENT_MODE_FIFO_RELAXED_KHR = 3,
		*/

	stream << "[";

	if (present_mode == VK_PRESENT_MODE_IMMEDIATE_KHR)
		stream << "VK_PRESENT_MODE_IMMEDIATE_KHR";
	else if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
		stream << "VK_PRESENT_MODE_MAILBOX_KHR";
	else if(present_mode == VK_PRESENT_MODE_FIFO_KHR)
		stream << "VK_PRESENT_MODE_FIFO_KHR";
	else if(present_mode == VK_PRESENT_MODE_FIFO_RELAXED_KHR)
		stream << "VK_PRESENT_MODE_FIFO_RELAXED_KHR";
	else {
		stream << "ERROR: Unexpected present mode";
	}

	stream << "]";
	return stream;
}


auto getVulkanQueueFlagNames(const int& flags)->std::string {

	auto ss = std::stringstream{};
	auto count = 0;

	ss << std::showbase // show the 0x prefix
		<< std::internal // fill between the prefix and the number
		<< std::setfill('0');

	ss << "[" << std::hex << flags << "]: " << std::dec << "[";

	if (flags & VK_QUEUE_GRAPHICS_BIT) {
		if (count++ > 0) ss << ", ";
		ss << "VK_QUEUE_GRAPHICS_BIT";
	}

	if (flags & VK_QUEUE_COMPUTE_BIT) {
		if (count++ > 0) ss << ", ";
		ss << "VK_QUEUE_COMPUTE_BIT";
	}

	if (flags & VK_QUEUE_TRANSFER_BIT) {
		if (count++ > 0) ss << ", ";
		ss << "VK_QUEUE_TRANSFER_BIT";
	}

	if (flags & VK_QUEUE_SPARSE_BINDING_BIT) {
		if (count++ > 0) ss << ", ";
		ss << "VK_QUEUE_SPARSE_BINDING_BIT";
	}
	ss << "]";

	return ss.str();
}

