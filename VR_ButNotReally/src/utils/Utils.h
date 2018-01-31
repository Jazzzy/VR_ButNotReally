#pragma once

/**

Prints the message in standard output that we are waiting
for a key press and returns after any keypress.

*/
auto pressToContinue() noexcept -> void;



enum class PrintOptions : int {
	full,
	none
};
