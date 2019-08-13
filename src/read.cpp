#include "gpio/gpio.hpp"
#include <boost/asio/io_context.hpp>
#include <iostream>
#include <vector>

using namespace Gpio;

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " chip_device offset [offset...]"
              << std::endl;
    return 1;
  }
  boost::asio::io_context ioc;
  Chip chip{ioc, argv[1]};
  std::vector<offset_t> offsets;
  offsets.reserve(argc - 2);
  for (int i = 2; i < argc; ++i) {
    offsets.push_back(std::atoi(argv[i]));
  }
  LineHandle h{chip, offsets, "test", In};
  uint64_t res = h.get();
  for (int i = 2; i < argc; ++i) {
    std::cout << (res & 1) << ' ';
    res >>= 1;
  }
  std::cout << std::endl;
  return 0;
}
