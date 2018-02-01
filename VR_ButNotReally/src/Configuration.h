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

	

	constexpr auto extensions_enabled =
#ifndef _DEBUG  //Release
		false;
#else           //Debug
		true;
#endif

	const std::vector<const char*> extensions 
#ifndef _DEBUG  //Release
	{
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};
#else           //Debug
	{
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};
#endif



	namespace gpu {
		
		constexpr auto discrete_gpu_bonus = 1000;
		constexpr auto integrated_gpu_bonus = 500;
		constexpr auto same_queue_family = 500;
		constexpr auto required_family_flags = VK_QUEUE_GRAPHICS_BIT;
		constexpr auto queue_amount = 1;
	}


}