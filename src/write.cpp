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
  LineHandle h{chip, offsets, "test", Out};
  while (std::cin) {
    uint64_t val;
    for (int i = 2; i < argc; ++i) {
      bool x;
      std::cin >> x;
      val = (val << 1) | x;
    }
    h.set(val);
  }
  return 0;
}
