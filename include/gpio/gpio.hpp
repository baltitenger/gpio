#ifndef GPIO_HPP
#define GPIO_HPP

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/read.hpp>
#include <chrono>
#include <linux/gpio.h>
#include <string>

namespace Gpio {
using uint = unsigned int;
using offset_t = uint8_t;
using Fd = boost::asio::posix::stream_descriptor;
using Ioc = boost::asio::io_context;

const offset_t MAX = GPIOHANDLES_MAX;
enum Edge {
  Rising = GPIOEVENT_REQUEST_RISING_EDGE,
  Falling = GPIOEVENT_REQUEST_FALLING_EDGE,
  Both = GPIOEVENT_REQUEST_BOTH_EDGES,
};
enum Dir {
  In = GPIOHANDLE_REQUEST_INPUT,
  Out = GPIOHANDLE_REQUEST_OUTPUT,
};
enum Flags {
  ActiveLow = GPIOHANDLE_REQUEST_ACTIVE_LOW,
  OpenDrain = GPIOHANDLE_REQUEST_OPEN_DRAIN,
  OpenSource = GPIOHANDLE_REQUEST_OPEN_SOURCE,
};

struct LineInfo {
  bool used;
  Dir dir;
  int flags;
  std::string name;
  std::string consumer;

  LineInfo(bool used, Dir dir, int flags, std::string name,
           std::string consumer) noexcept
      : used{used}, dir{dir}, flags{flags}, name{name}, consumer{consumer} {}
};

namespace ioctl {
template <typename T> auto ioctl(Fd &fd, T command) {
  fd.io_control(command);
  return command.data_;
}
struct ChipInfo {
  gpiochip_info data_;

  int name() const { return GPIO_GET_CHIPINFO_IOCTL; }
  void *data() { return &data_; }
};
struct LineInfo {
  gpioline_info data_;

  int name() const { return GPIO_GET_LINEINFO_IOCTL; }
  void *data() { return &data_; }
  LineInfo(offset_t offset) { data_.line_offset = offset; }
};
struct LineHandle {
  gpiohandle_request data_;

  int name() const { return GPIO_GET_LINEHANDLE_IOCTL; }
  void *data() { return &data_; }
  template <typename Offsets = std::initializer_list<offset_t>>
  LineHandle(const Offsets &offsets, const std::string &consumer, Dir dir,
             int flags = 0, uint64_t defaults = -1) {
    if (offsets.size() > MAX) {
      throw std::runtime_error("Maximum number of requested lines exceeded!");
    } else if (dir == In && (flags & (OpenDrain | OpenSource))) {
      throw std::runtime_error(
          "Cant't be open drain or open source while inputting!");
    } else if ((flags & OpenDrain) && (flags & OpenSource)) {
      throw std::runtime_error(
          "Cant't be open drain and open source at the same time!");
    }

    data_.flags = dir | flags;
    data_.lines = offsets.size();

    auto offset = offsets.begin();
    for (uint i = 0; i < offsets.size(); ++i, ++offset) {
      data_.lineoffsets[i] = *offset;
      data_.default_values[i] = defaults & 1;
      defaults >>= 1;
    }

    if (!consumer.empty()) {
      std::copy(consumer.begin(), consumer.end(), data_.consumer_label);
      data_.consumer_label[consumer.size()] = '\0';
    }
  }
};
struct GetLineValues {
  gpiohandle_data data_;

  int name() const { return GPIOHANDLE_GET_LINE_VALUES_IOCTL; }
  void *data() { return &data_; }
};
struct SetLineValues {
  gpiohandle_data data_;

  int name() const { return GPIOHANDLE_SET_LINE_VALUES_IOCTL; }
  void *data() { return &data_; }
  SetLineValues(uint64_t values, offset_t count = MAX) {
    for (uint i = 0; i < count; ++i) {
      data_.values[i] = values & 1;
      values >>= 1;
    }
  }
};
struct EventHandle {
  gpioevent_request data_;

  int name() const { return GPIO_GET_LINEEVENT_IOCTL; }
  void *data() { return &data_; }
  template <typename Offsets = std::initializer_list<offset_t>>
  EventHandle(offset_t offset, const std::string &consumer, int flags = 0,
              Edge events = Both) {
    if (flags & (OpenDrain | OpenSource)) {
      throw std::runtime_error(
          "Cant't be open drain or open source while inputting!");
    }

    data_.handleflags = In | flags;
    data_.eventflags = events;

    if (!consumer.empty()) {
      std::copy(consumer.begin(), consumer.end(), data_.consumer_label);
      data_.consumer_label[consumer.size()] = '\0';
    }

    data_.lineoffset = offset;
  }
};
} // namespace ioctl

struct Chip {
  Fd fd;
  std::string name;
  std::string label;
  offset_t numLines;

  Chip(Ioc &ioc, const std::string &devName)
      : fd{ioc, open(("/dev/" + devName).c_str(), 0)} {
    gpiochip_info info = ioctl::ioctl(fd, ioctl::ChipInfo{});

    name = info.name;
    numLines = info.lines;

    if (info.label[0] == '\0') {
      label = "unknown";
    } else {
      label = info.label;
    }
  }

  LineInfo info(offset_t offset) {
    gpioline_info info = ioctl::ioctl(fd, ioctl::LineInfo{offset});

    return {bool(info.flags & GPIOLINE_FLAG_KERNEL),
            info.flags & GPIOLINE_FLAG_IS_OUT ? Out : In,
            ((info.flags & GPIOLINE_FLAG_ACTIVE_LOW) ? ActiveLow : 0) |
                ((info.flags & GPIOLINE_FLAG_OPEN_DRAIN) ? OpenDrain : 0) |
                ((info.flags & GPIOLINE_FLAG_OPEN_SOURCE) ? OpenSource : 0),
            info.name, info.consumer};
  }
};

class LineHandle {
  Fd fd;

public:
  offset_t count;

  template <typename Offsets = std::initializer_list<offset_t>>
  LineHandle(Chip &chip, const Offsets &offsets, const std::string &consumer,
             Dir dir, int flags = 0, uint64_t defaults = -1)
      : fd{chip.fd.get_executor().context(),
           ioctl::ioctl(chip.fd, ioctl::LineHandle{offsets, consumer, dir,
                                                   flags, defaults})
               .fd},
        count{static_cast<offset_t>(offsets.size())} {}

  uint64_t get() {
    gpiohandle_data data = ioctl::ioctl(fd, ioctl::GetLineValues{});

    uint64_t res = 0;
    for (uint i = 0; i < count; ++i) {
      res |= data.values[i] << i;
    }
    return res;
  }

  void set(uint64_t values) {
    ioctl::ioctl(fd, ioctl::SetLineValues{values, count});
  }
};

struct Event {
  std::chrono::time_point<std::chrono::system_clock> timestamp;
  Edge edge;

  Event(std::chrono::time_point<std::chrono::system_clock> timestamp = {},
        Edge edge = Both) noexcept
      : timestamp{timestamp}, edge{edge} {}
  operator bool() const noexcept {
    return timestamp.time_since_epoch().count() != 0;
  }
};

class EventHandle {
  Fd fd;

public:
  EventHandle(Chip &chip, offset_t offset, const std::string &consumer,
              int flags = 0, Edge events = Both)
      : fd{chip.fd.get_executor().context(),
           ioctl::ioctl(chip.fd,
                        ioctl::EventHandle{offset, consumer, flags, events})
               .fd} {}

  Event read() {
    gpioevent_data e;
    boost::system::error_code ec;
    boost::asio::read(fd, boost::asio::buffer(&e, sizeof(e)), ec);
    if (ec) {
      if (ec == boost::asio::error::would_block) {
        return {};
      } else {
        throw boost::system::system_error{ec};
      }
    }
    return {std::chrono::time_point<std::chrono::system_clock>(
                std::chrono::nanoseconds(e.timestamp)),
            (e.id == GPIOEVENT_EVENT_RISING_EDGE) ? Rising : Falling};
  }

  void cancel() { fd.cancel(); }

  void cancel(boost::system::error_code &ec) { fd.cancel(ec); }

  template <typename WaitHandler> auto async_wait(WaitHandler &&handler) {
    return fd.async_wait(fd.wait_read, handler);
  }

  void wait() { fd.wait(fd.wait_read); }

  void wait(boost::system::error_code &ec) { fd.wait(fd.wait_read, ec); }
};

} // namespace Gpio

#endif // GPIO_HPP
