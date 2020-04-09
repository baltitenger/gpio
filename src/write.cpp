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
	for (offset_t i = 2; i < argc; ++i) {
		offsets.push_back(std::atoi(argv[i]));
	}
	LineHandle h(ioc, chip, offsets, Out, argv[0]);
	while (true) {
		uint64_t val = 0;
		for (int i = 2; i < argc; ++i) {
			bool x;
			std::cin >> x;
			if (!std::cin) {
				return 0;
			}
			val = (val << 1) | x;
		}
		h.set(val);
	}
}

// vim: ts=2 sw=0 noet
