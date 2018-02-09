#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <locale>
#include <codecvt>

auto main(int argc, char** argv) -> int {

	if (argc != 3) {
		std::cerr << "We need a file to get the binary from and a name for the array" << std::endl;
		return EXIT_FAILURE;
	}

	auto file = std::wifstream(argv[1], std::ios::ate | std::ios::in | std::ios_base::binary);

	file.imbue(std::locale(file.getloc(),
		new std::codecvt_utf16<wchar_t, 0x10ffff, std::consume_header>));

	if (!file.is_open()) {
		std::cerr << "We couldn't open the provided file" << std::endl;
		return EXIT_FAILURE;
	}

	auto file_size = (size_t)(file.tellg());
	auto buffer = std::vector<char>(file_size);
	auto array_size = file_size / sizeof(wchar_t);

	file.seekg(0);
	
	std::cout << "std::array<wchar_t, " << array_size  << "> " << argv[2] << " = {" << std::endl;

	auto c = wchar_t{};
	auto count = 0ll;
	while (file.read(&c, 1)) {
		std::cout << "0x" << std::setw(2) << std::setfill('0') << std::hex << int(c) << ", ";
		if (!(++count % 10)) {
			std::cout << std::endl;
		}
	}

	std::cout << "};" << std::endl << std::endl;

	//std::cout << "constexpr auto " << argv[2] << "_size = sizeof(" << argv[2] << ")/sizeof(*" << argv[2] << ");" << std::endl;

	return EXIT_SUCCESS;

}