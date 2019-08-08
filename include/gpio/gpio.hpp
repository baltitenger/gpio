#ifndef GPIO_HPP
#define GPIO_HPP

#include <chrono>
#include <fcntl.h>
#include <linux/gpio.h>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <unistd.h>

namespace Gpio {
using uint = unsigned int;
using offset_t = uint8_t;

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

class Fd {
  struct FdInner {
    int fd;
    FdInner(int fd) noexcept : fd{fd} {}
    FdInner(const FdInner &) = delete;
    FdInner &operator=(const FdInner &) = delete;
    FdInner(FdInner &&) = delete;
    FdInner &operator=(FdInner &&) = delete;
    ~FdInner() { close(fd); }
  };
  std::shared_ptr<FdInner> fd;

public:
  Fd(int fd = -1) noexcept : fd(std::make_shared<FdInner>(fd)) {}
  operator int() const noexcept { return fd->fd; }
};

struct Chip {
  Fd fd;
  std::string name;
  std::string label;
  offset_t numLines;

  Chip(const std::string &devName) : fd(open(("/dev/" + devName).c_str(), 0)) {
    if (fd < 0) {
      throw std::runtime_error("Failed opening gpio device.");
    }

    gpiochip_info info;
    if (ioctl(fd, GPIO_GET_CHIPINFO_IOCTL, &info) < 0) {
      throw std::runtime_error("Failed getting chip info.");
    }

    name = info.name;
    numLines = info.lines;

    if (info.label[0] == '\0') {
      label = "unknown";
    } else {
      label = info.label;
    }
  }

  LineInfo info(offset_t offset) {
    gpioline_info info;
    info.line_offset = offset;
    if (ioctl(fd, GPIO_GET_LINEINFO_IOCTL, &info) < 0) {
      throw std::runtime_error("Failed getting line info.");
    }

    return {bool(info.flags & GPIOLINE_FLAG_KERNEL),
            info.flags & GPIOLINE_FLAG_IS_OUT ? Out : In,
            (info.flags & GPIOLINE_FLAG_ACTIVE_LOW)
                ? ActiveLow
                : 0 | (info.flags & GPIOLINE_FLAG_OPEN_DRAIN)
                      ? OpenDrain
                      : 0 | (info.flags & GPIOLINE_FLAG_OPEN_SOURCE)
                            ? OpenSource
                            : 0,
            info.name, info.consumer};
  }
};

class LineHandle {
  Fd fd;

public:
  offset_t count;

  template <typename Offsets = std::initializer_list<offset_t>>
  LineHandle(const Chip &chip, const Offsets &offsets,
             const std::string &consumer, Dir dir, int flags = 0,
             uint64_t defaults = -1)
      : count(offsets.size()) {
    if (offsets.size() > MAX) {
      throw std::runtime_error("Maximum number of requested lines exceeded!");
    } else if (dir == In && (flags & (OpenDrain | OpenSource))) {
      throw std::runtime_error(
          "Cant't be open drain or open source while inputting!");
    } else if ((flags & OpenDrain) && (flags & OpenSource)) {
      throw std::runtime_error(
          "Cant't be open drain and open source at the same time!");
    }

    gpiohandle_request req;
    req.flags = dir | flags;
    req.lines = offsets.size();

    auto offset = offsets.begin();
    for (uint i = 0; i < offsets.size(); ++i, ++offset) {
      req.lineoffsets[i] = *offset;
      req.default_values[i] = defaults & 1;
      defaults >>= 1;
    }

    if (!consumer.empty()) {
      std::copy(consumer.begin(), consumer.end(), req.consumer_label);
      req.consumer_label[consumer.size()] = '\0';
    }

    if (ioctl(chip.fd, GPIO_GET_LINEHANDLE_IOCTL, &req) < 0) {
      throw std::logic_error("Failed requesting handle.");
    }

    if (!req.fd) {
      throw std::logic_error("Some sort of error.");
    }

    fd = req.fd;
  }

  uint64_t get() {
    gpiohandle_data data;
    if (ioctl(fd, GPIOHANDLE_GET_LINE_VALUES_IOCTL, &data) < 0) {
      throw std::runtime_error("Failed getting line values.");
    }

    uint64_t res = 0;
    for (uint i = 0; i < count; ++i) {
      res |= data.values[i] << i;
    }
    return res;
  }

  void set(uint64_t values) {
    gpiohandle_data data;
    for (uint i = 0; i < count; ++i) {
      data.values[i] = values & 1;
      values >>= 1;
    }
    if (ioctl(fd, GPIOHANDLE_SET_LINE_VALUES_IOCTL, &data) < 0) {
      throw std::logic_error("Failed setting line values.");
    }
  }
};

struct Event {
  std::chrono::time_point<std::chrono::system_clock> timestamp;
  Edge edge;
  Event(std::chrono::time_point<std::chrono::system_clock> timestamp = {},
        Edge edge = Both) noexcept
      : timestamp(timestamp), edge(edge) {}
  operator bool() const noexcept {
    return timestamp.time_since_epoch().count() != 0;
  }
};

class EventHandle {
  Fd fd;

public:
  EventHandle(const Chip &chip, offset_t offset, const std::string &consumer,
              int flags = 0, Edge events = Both) {
    if (flags & (OpenDrain | OpenSource)) {
      throw std::runtime_error(
          "Cant't be open drain or open source while inputting!");
    }

    gpioevent_request req;
    req.handleflags = In | flags;
    req.eventflags = events;

    if (!consumer.empty()) {
      std::copy(consumer.begin(), consumer.end(), req.consumer_label);
      req.consumer_label[consumer.size()] = '\0';
    }

    req.lineoffset = offset;

    if (ioctl(chip.fd, GPIO_GET_LINEEVENT_IOCTL, &req) < 0) {
      throw std::runtime_error("Failed requesting events.");
    }

    if (!req.fd) {
      throw std::runtime_error("Some sort of error");
    }

    fd = req.fd;
  }

  template <typename Pins = std::initializer_list<EventHandle>>
  static uint64_t
  wait(const Pins &pins,
       std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) {
    pollfd req[pins.size()];
    auto pin = pins.begin();
    for (offset_t i = 0; i < pins.size(); ++i, ++pin) {
      req[i].fd = pin->fd;
      req[i].events = POLLIN;
    }

    int res = poll(req, pins.size(), timeout.count());
    if (res < 0) {
      throw std::runtime_error("Poll error.");
    }
    if (res == 0) {
      return 0;
    }

    uint64_t ret = 0;
    for (offset_t i = 0; i < pins.size(); ++i) {
      if (req[i].revents & POLLIN) {
        ret |= 1 << i;
      }
    }
    return ret;
  }

  uint64_t
  wait(std::chrono::milliseconds timeout = std::chrono::milliseconds::zero()) {
    return EventHandle::wait({*this}, timeout);
  }

  Event read() {
    if (wait() == 0) {
      return {};
    }
    gpioevent_data e;
    using ::read;
    if (read(fd, &e, sizeof(e)) != sizeof(e)) {
      throw std::runtime_error("Shit happened");
    }
    return {std::chrono::time_point<std::chrono::system_clock>(
                std::chrono::nanoseconds(e.timestamp)),
            (e.id == GPIOEVENT_EVENT_RISING_EDGE) ? Rising : Falling};
  }
};

} // namespace Gpio

#endif // GPIO_HPP
