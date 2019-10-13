#ifndef GPIO_HPP
#define GPIO_HPP

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/read.hpp>
#include <boost/system/error_code.hpp>
#include <chrono>
#include <linux/gpio.h>
#include <string>

namespace Gpio {
using uint = unsigned int;
using offset_t = uint8_t;
using Fd = boost::asio::posix::stream_descriptor;
using Ioc = boost::asio::io_context;
using Ec = boost::system::error_code;

const offset_t MAX = GPIOHANDLES_MAX;
enum Edge : uint32_t {
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
  uint flags;
  std::string name;
  std::string consumer;

  LineInfo(const gpioline_info &info) noexcept
      : used{static_cast<bool>(info.flags & GPIOLINE_FLAG_KERNEL)},
        dir{info.flags & GPIOLINE_FLAG_IS_OUT ? Out : In},
        flags{info.flags & (ActiveLow | OpenDrain | OpenSource)},
        name{info.name}, consumer{info.consumer} {}
};

namespace ioctl {
template <typename T> auto ioctl(Fd &fd, T command) {
  fd.io_control(command);
  return command.data_;
}
struct ChipInfo {
  gpiochip_info data_;

  constexpr int name() const noexcept { return GPIO_GET_CHIPINFO_IOCTL; }
  constexpr void *data() noexcept { return &data_; }
};
struct LineInfo {
  gpioline_info data_;

  constexpr int name() const noexcept { return GPIO_GET_LINEINFO_IOCTL; }
  constexpr void *data() noexcept { return &data_; }
  constexpr LineInfo(offset_t offset) noexcept : data_{.line_offset = offset} {}
};
struct LineHandle {
  gpiohandle_request data_;

  constexpr int name() const noexcept { return GPIO_GET_LINEHANDLE_IOCTL; }
  constexpr void *data() noexcept { return &data_; }
  template <typename Offsets = std::initializer_list<offset_t>>
  constexpr LineHandle(const Offsets &offsets, const std::string &consumer, Dir dir,
             uint flags = 0, uint64_t defaults = -1) {
    if (offsets.size() > MAX) {
      throw std::runtime_error{"Maximum number of requested lines exceeded!"};
    } else if (dir == In && (flags & (OpenDrain | OpenSource))) {
      throw std::runtime_error{"Cant't be open drain or open source while inputting!"};
    } else if ((flags & OpenDrain) && (flags & OpenSource)) {
      throw std::runtime_error{"Cant't be open drain and open source at the same time!"};
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

  constexpr int name() const noexcept { return GPIOHANDLE_GET_LINE_VALUES_IOCTL; }
  constexpr void *data() noexcept { return &data_; }
};
struct SetLineValues {
  gpiohandle_data data_;

  constexpr int name() const noexcept { return GPIOHANDLE_SET_LINE_VALUES_IOCTL; }
  constexpr void *data() noexcept { return &data_; }
  constexpr SetLineValues(uint64_t values, offset_t count = MAX) noexcept : data_{} {
    for (uint i = 0; i < count; ++i) {
      data_.values[i] = values & 1;
      values >>= 1;
    }
  }
};
struct EventHandle {
  gpioevent_request data_;

  constexpr int name() const noexcept { return GPIO_GET_LINEEVENT_IOCTL; }
  constexpr void *data() noexcept { return &data_; }
  template <typename Offsets = std::initializer_list<offset_t>>
  constexpr EventHandle(offset_t offset, const std::string &consumer, uint flags = 0,
              Edge events = Both) {
    if (flags & (OpenDrain | OpenSource)) {
      throw std::runtime_error{"Cant't be open drain or open source while inputting!"};
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
  Ioc &ioc;
  Fd fd;
  std::string name;
  std::string label;
  offset_t numLines;

  Chip(Ioc &ioc, const std::string &devName)
      : Chip{ioc, {ioc, open(("/dev/" + devName).c_str(), 0)}} {}

  Chip(Ioc &ioc, Fd &&fd)
    : Chip{ioc, std::move(fd), ioctl::ioctl(fd, ioctl::ChipInfo{})} {}

  Chip(Ioc &ioc, Fd &&fd, const gpiochip_info &info)
    : ioc{ioc}, fd{std::move(fd)}, name{info.name}, label{info.label}, numLines{static_cast<offset_t>(info.lines)} {}

  LineInfo info(offset_t offset) {
    return {ioctl::ioctl(fd, ioctl::LineInfo{offset})};
  }
};

class LineHandle {
  Fd fd;

public:
  offset_t count;

  template <typename Offsets = std::initializer_list<offset_t>>
  LineHandle(Chip &chip, ioctl::LineHandle &&params)
      : fd{chip.ioc, ioctl::ioctl(chip.fd, params).fd},
        count{static_cast<offset_t>(params.data_.lines)} {}

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

struct Event { // should be compatible with gpioevent_data
  using Timestamp = std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<uint64_t, std::nano>>;
  Timestamp timestamp;
  Edge edge;

  constexpr operator bool() const noexcept {
    return timestamp.time_since_epoch().count() != 0;
  }
};

class EventHandle {
  Fd fd;

public:
  EventHandle(Chip &chip, ioctl::EventHandle &&params)
      : fd{chip.ioc, ioctl::ioctl(chip.fd, params).fd} {}

  Event read() {
    Event e;
    Ec ec;
    boost::asio::read(fd, boost::asio::buffer(&e, sizeof(e)), ec);
    if (ec) {
      if (ec == boost::asio::error::would_block) {
        return {};
      } else {
        throw boost::system::system_error{ec};
      }
    }
    return e;
  }

  template <typename ReadHandler>
  void async_read(Event &e, ReadHandler &&handler) {
    boost::asio::async_read(fd, boost::asio::buffer(&e, sizeof(e)), handler);
  }

  void cancel() { fd.cancel(); }

  void cancel(Ec &ec) { fd.cancel(ec); }

  template <typename WaitHandler> auto async_wait(WaitHandler &&handler) {
    return fd.async_wait(fd.wait_read, handler);
  }

  void wait() { fd.wait(fd.wait_read); }

  void wait(Ec &ec) { fd.wait(fd.wait_read, ec); }
};

} // namespace Gpio

#endif // GPIO_HPP
