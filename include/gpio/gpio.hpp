#ifndef GPIO_HPP
#define GPIO_HPP

#include <boost/asio/posix/stream_descriptor.hpp>
#include <boost/asio/read.hpp>
#include <chrono>
#include <linux/gpio.h>
#include <string>
#include <string_view>
#include <cassert>

namespace Gpio {
namespace net = boost::asio;
using offset_t = uint8_t;
using Fd = net::posix::stream_descriptor;
using Ioc = net::io_context;
using Ec = boost::system::error_code;

enum Edge {
	RisingEdge  = GPIOEVENT_REQUEST_RISING_EDGE,
	FallingEdge = GPIOEVENT_REQUEST_FALLING_EDGE,
	BothEdges   = GPIOEVENT_REQUEST_BOTH_EDGES,
};
enum Dir {
	In  = GPIOHANDLE_REQUEST_INPUT,
	Out = GPIOHANDLE_REQUEST_OUTPUT,
};
enum Flags {
	ActiveLow  = GPIOHANDLE_REQUEST_ACTIVE_LOW,
	OpenDrain  = GPIOHANDLE_REQUEST_OPEN_DRAIN,
	OpenSource = GPIOHANDLE_REQUEST_OPEN_SOURCE,
};

class ChipInfo {
	gpiochip_info info;
	constexpr ChipInfo(gpiochip_info info) noexcept : info(info) {}
	friend class Chip;

public:
	constexpr const char *name() const noexcept { return info.name; }
	constexpr const char *label() const noexcept { return info.label; }
	constexpr offset_t lines() const noexcept { return info.lines; }
};

class LineInfo {
	gpioline_info info;
	constexpr LineInfo(gpioline_info info) noexcept : info(info) {}
	friend class Chip;

public:
	constexpr bool offset() const noexcept { return info.line_offset; }
	constexpr bool used() const noexcept { return info.flags & GPIOLINE_FLAG_KERNEL; }
	constexpr Dir dir() const noexcept { return info.flags & GPIOLINE_FLAG_IS_OUT ? Out : In; }
	constexpr uint flags() const noexcept { return info.flags & (ActiveLow | OpenDrain | OpenSource); }
	constexpr const char *name() const noexcept { return info.name; }
	constexpr const char *consumer() const noexcept { return info.consumer; }
};

class Chip {
	Fd fd;
	friend class LineHandle;
	friend class EventHandle;

public:
	Chip(Ioc &ioc, std::string_view devName);
	ChipInfo info();
	LineInfo lineInfo(offset_t);
};

class LineHandle {
	Fd fd;
	offset_t count;

public:
	static constexpr offset_t MAX = GPIOHANDLES_MAX;

	LineHandle(Ioc &ioc, Chip &chip, gpiohandle_request req);
	template <typename Offsets = std::initializer_list<offset_t>>
	LineHandle(Ioc &ioc, Chip &chip, const Offsets &offsets, Dir dir
		,	std::string_view consumer = "", uint flags = 0, uint64_t defaults = -1)
		:	LineHandle(ioc, chip, [&]() constexpr {
			assert(offsets.size() <= MAX);
			assert(!(dir == In && (flags & (OpenDrain | OpenSource))));
			assert(!(flags & (OpenDrain | OpenSource)));
			assert(consumer.length() < 32);
			gpiohandle_request req;
			offset_t i = 0; auto it = offsets.begin();
			while (i < offsets.size()) {
				req.lineoffsets[i] = *it;
				req.default_values[i] = defaults & 1;
				++i; ++it; defaults >>= 1;
			}
			req.flags = dir | flags;
			std::memcpy(req.consumer_label, consumer.data(), consumer.length());
			req.consumer_label[consumer.length()] = '\0';
			req.lines = offsets.size();
			req.fd = -1;
			return req;
		}()) {}

	uint64_t get();
	void set(uint64_t values);
};

class Event {
	gpioevent_data data;
	friend class EventHandle;

public:
	using Clock = std::chrono::system_clock;
	using Duration = std::chrono::duration<uint64_t, std::nano>;
	using Timestamp = std::chrono::time_point<Clock, Duration>;

	constexpr Timestamp timestamp() const noexcept { return Timestamp(Duration(data.timestamp)); }
	constexpr Edge edge() const noexcept { return Edge(data.id); }
	constexpr operator bool() const noexcept { return data.timestamp != 0; }
};

class EventHandle {
	Fd fd;

public:
	EventHandle(Ioc &ioc, Chip &chip, offset_t offset, Edge events = BothEdges,
		std::string_view consumer = "", uint flags = 0);

	Event read();
	Event read(Ec &ec);

	void wait() { fd.wait(fd.wait_read); }
	void wait(Ec &ec) { fd.wait(fd.wait_read, ec); }

	void cancel() { fd.cancel(); }
	void cancel(Ec &ec) { fd.cancel(ec); }

	template <typename ReadHandler>
	auto async_read(Event &e, ReadHandler &&handler) {
		return net::async_read(fd, net::buffer(&e.data, sizeof(e.data)),
			std::forward<ReadHandler>(handler));
	}
	template <typename WaitHandler>
	auto async_wait(WaitHandler &&handler) {
		return fd.async_wait(fd.wait_read, std::forward<WaitHandler>(handler));
	}
};

} // namespace Gpio

#endif // GPIO_HPP
// vim: ts=2 sw=0 noet
