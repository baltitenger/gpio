#include "gpio/gpio.hpp"
#include <boost/asio/io_context.hpp>
#include <iomanip>
#include <iostream>

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
    h.wait();
    Event e{h.read()};
    std::time_t time = std::chrono::system_clock::to_time_t(e.timestamp);
    std::cout << "Event at " << std::put_time(std::localtime(&time), "%F %T")
              << ": " << (e.edge == Rising ? "rising" : "falling") << " edge";
  }
  return 0;
}
