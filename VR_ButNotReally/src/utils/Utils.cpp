#include "./Utils.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
auto pressToContinue() noexcept -> void { system("pause"); }
#else
auto pressToContinue() noexcept -> void { system("read"); }
#endif

auto readFileToChars(const std::string& name)->std::vector<char> {
	auto file = std::ifstream(name, std::ios::ate, std::ios::binary);

	if (!file.is_open()) {
		auto ss = std::stringstream{};
		ss << "We could not read the file [" << name << "]"; 
		throw new std::runtime_error(ss.str());
	}

	// @@DOING: Reading the file size, we need to cast it to size_t
	// https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Shader_modules
	auto file_size = file.tellg();

}