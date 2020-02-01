#include "gpio/gpio.hpp"
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <vector>

using namespace Gpio;

int main(int argc, char *argv[]) {
	if (argc < 3) {
		std::cerr << "Usage: " << argv[0] << " gpiochip_name offset [offset...]\n";
		return 1;
	}
	boost::asio::io_context ioc;
	Chip chip(ioc, argv[1]);
	std::vector<offset_t> offsets;
	offsets.reserve(argc - 2);
	for (int i = 2; i < argc; ++i) {
		offsets.push_back(std::atoi(argv[i]));
	}
	LineHandle h(chip, {offsets, "test", In});
	while (std::cin) {
		uint64_t res = h.get();
		for (int i = 2; i < argc; ++i) {
			std::cout << (res & 1) << ' ';
			res >>= 1;
		}
		std::cin.ignore();
	}
}
