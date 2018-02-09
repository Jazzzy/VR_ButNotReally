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

auto readBinaryArrayToChars(const wchar_t* arr, size_t size)->std::vector<char>;

enum class PrintOptions : int {
	full,
	none
};
