#include "gpio/gpio.hpp"
#include <boost/asio/io_context.hpp>
#include <iomanip>
#include <iostream>
#include <ratio>

using namespace Gpio;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Usage: " << argv[0] << " chip_device offset" << std::endl;
    return 1;
  }
  boost::asio::io_context ioc;
  Chip chip{ioc, argv[1]};
  offset_t offset = atoi(argv[2]);
  EventHandle h{chip, offset, "test"};
  while (true) {
    using namespace std::chrono;
    h.wait();
    Event e{h.read()};
    auto s{time_point_cast<seconds>(e.timestamp)};
    auto frac{duration_cast<duration<int, std::milli>>(e.timestamp - s)};
    time_t time_c = system_clock::to_time_t(e.timestamp);
    tm *tm_c = std::localtime(&time_c);
    std::printf("Event at %02d:%02d:%02d.%06d: %s edge\n", tm_c->tm_hour,
                tm_c->tm_min, tm_c->tm_sec, frac.count(),
                e.edge == Rising ? "Rising" : "Falling");
  }
  return 0;
}
