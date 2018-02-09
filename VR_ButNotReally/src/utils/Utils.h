#pragma once

#include <string>
#include <vector>
#include <array>


/**

Prints the message in standard output that we are waiting
for a key press and returns after any keypress.

*/
auto pressToContinue() noexcept -> void;

auto readFileToChars(const std::string& name)->std::vector<char>;

template<typename T>
auto readBinaryArrayToChars(T arr)->std::vector<char> {

	auto buffer = std::vector<char>(arr.size()*(sizeof(wchar_t) / sizeof(char)));

	std::cout << "Reading binary array with size: " << buffer.size() << std::endl << std::endl;

	memcpy(buffer.data(), arr.data(), arr.size() * sizeof(wchar_t));

	for (size_t i = 0; i < buffer.size(); i += 2) {
		std::swap(buffer[i], buffer[i + 1]);
	}

	return buffer;
}


enum class PrintOptions : int {
	full,
	none
};
