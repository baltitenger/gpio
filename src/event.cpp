#include "gpio/gpio.hpp"
#include <cstdio>

using namespace Gpio;

int main(int argc, char *argv[]) {
	if (argc != 3) {
		std::fprintf(stderr, "Usage: %s gpiochip_name offset\n", argv[0]);
		return 1;
	}
	Ioc ioc;
	Chip chip(ioc, argv[1]);
	offset_t offset(atoi(argv[2]));
	EventHandle h(ioc, chip, offset, BothEdges, argv[0]);
	while (true) {
		using namespace std::chrono;
		h.wait();
		Event e(h.read());
		auto s(time_point_cast<seconds>(e.timestamp()));
		auto frac(duration_cast<duration<uint, std::micro>>(e.timestamp() - s));
		auto time = system_clock::to_time_t(e.timestamp());
		auto &tm = *std::localtime(&time);
		std::printf("Event at %02d:%02d:%02d.%06d: %s edge\n",
			tm.tm_hour, tm.tm_min, tm.tm_sec, frac.count(),
			e.edge() == RisingEdge ? "Rising" : "Falling");
	}
}

// vim: ts=2 sw=0 noet
