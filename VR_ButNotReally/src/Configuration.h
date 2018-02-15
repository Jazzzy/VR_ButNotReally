#pragma once

#include <string>
#include <vulkan/vulkan.h>

#pragma warning(disable: 26426)

namespace config {

	constexpr auto app_name{ "VR: But not really" };
	constexpr auto engine_name{ "No engine" };
	constexpr auto initial_window_width{ 800 };
	constexpr auto initial_window_heigth{ 600 };
	constexpr auto major_version{ 1 };
	constexpr auto minor_version{ 0 };
	constexpr auto patch_version{ 0 };

	constexpr auto validation_layers_enabled =
#ifndef _DEBUG  //Release
		false;
#else           //Debug
		true;
#endif

	const std::vector<const char*> validation_layers
	{
		"VK_LAYER_LUNARG_standard_validation"
	};



	constexpr auto instance_extensions_enabled =
#ifndef _DEBUG  //Release
		false;
#else           //Debug
		true;
#endif

	const std::vector<const char*> instance_extensions
#ifndef _DEBUG  //Release
	{
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};
#else           //Debug
	{
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};
#endif

	const std::vector<const char *> device_extensions{
		VK_KHR_SWAPCHAIN_EXTENSION_NAME
	};

	const std::vector<VkPresentModeKHR> preferred_present_modes_sorted{
		VK_PRESENT_MODE_MAILBOX_KHR,
		VK_PRESENT_MODE_IMMEDIATE_KHR,
		VK_PRESENT_MODE_FIFO_KHR,
		VK_PRESENT_MODE_FIFO_RELAXED_KHR
	};

	namespace gpu {

		constexpr auto discrete_gpu_bonus = 1000;
		constexpr auto integrated_gpu_bonus = 500;
		constexpr auto same_queue_family = 500;
		constexpr auto queue_amount = 1;
	}

	const auto resource_path = std::string{ "./res/" };
	const auto shader_path = std::string{ "./res/shaders/" };

	const auto clear_color = VkClearValue{ 0.0f, 0.0f, 0.0f, 1.0f };

}