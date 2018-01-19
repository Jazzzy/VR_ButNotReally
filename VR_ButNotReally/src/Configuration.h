#pragma once

#include <string>
#include <vulkan/vulkan.h>

#pragma warning(disable: 26426)

namespace config {

	const auto app_name{ "VR: But not really" };
	const auto engine_name{ "No engine" };
	const auto initial_window_width{ 800 };
	const auto initial_window_heigth{ 600 };
	const auto major_version{ 1 };
	const auto minor_version{ 0 };
	const auto patch_version{ 0 };
	
	const auto validation_layers_enabled =
#ifdef NDEBUG
		false;
#else
		true;
#endif

	const std::vector<const char*> validation_layers
#ifdef NDEBUG
	{
		"VK_LAYER_LUNARG_standard_validation"
	};
#else
	{
		"VK_LAYER_LUNARG_standard_validation"
	};
#endif
	

	const auto extensions_enabled =
#ifdef NDEBUG
		false;
#else
		true;
#endif

	const std::vector<const char*> extensions 
#ifdef NDEBUG
	{
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};
#else
	{
		VK_EXT_DEBUG_REPORT_EXTENSION_NAME
	};
#endif

}