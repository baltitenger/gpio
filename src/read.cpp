#include "gpio/gpio.hpp"
#include <iostream>
#include <vector>

using namespace Gpio;

int main(int argc, char *argv[]) {
	if (argc < 3) {
		std::fprintf(stderr, "Usage: %s gpiochip_name offset [...]\n", argv[0]);
		return 1;
	}
	Ioc ioc;
	Chip chip(ioc, argv[1]);
	std::vector<offset_t> offsets;
	offsets.reserve(argc - 2);
	for (int i = 2; i < argc; ++i) {
		offsets.push_back(std::atoi(argv[i]));
	}
	LineHandle h(ioc, chip, offsets, In, argv[0]);
	while (std::cin) {
		uint64_t res = h.get();
		for (int i = 2; i < argc; ++i) {
			std::cout << (res & 1) << ' ';
			res >>= 1;
		}
		std::cin.ignore();
	}
}

// vim: ts=2 sw=0 noet
