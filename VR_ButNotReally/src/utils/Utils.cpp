#include "./Utils.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <gsl/gsl>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32) && !defined(__CYGWIN__)
auto pressToContinue() noexcept -> void { system("pause"); }
#else
auto pressToContinue() noexcept -> void { system("read"); }
#endif

auto readFileToChars(const std::string& name)->std::vector<char> {
	auto file = std::ifstream(name, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		auto ss = std::stringstream{};
		ss << "We could not read the file [" << name << "]";
		throw std::runtime_error(ss.str());
	}

	auto file_size = gsl::narrow<size_t>(file.tellg());
	auto buffer = std::vector<char>(file_size);

	file.seekg(0);
	file.read(buffer.data(), file_size);
	file.close();

	std::cout << "Reading file [" << name << "] with size: " << buffer.size() << std::endl << std::endl;

	return buffer;
}