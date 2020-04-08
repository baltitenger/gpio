#include "gpio/gpio.hpp"

namespace Gpio {

template <uint Name, typename T>
struct IoControlCommand {
	T data_;
	constexpr int name() const noexcept { return Name; }
	constexpr void *data() noexcept { return &data_; }
};

Chip::Chip(Ioc &ioc, std::string_view devName)
	: fd(ioc, open(std::string("/dev/").append(devName).c_str(), 0)) {}

ChipInfo Chip::info() {
	IoControlCommand<GPIO_GET_CHIPINFO_IOCTL, gpiochip_info> cmd;
	fd.io_control(cmd);
	return ChipInfo(std::move(cmd.data_));
}

LineInfo Chip::lineInfo(offset_t offset) {
	IoControlCommand<GPIO_GET_LINEINFO_IOCTL, gpioline_info> cmd;
	cmd.data_.line_offset = offset;
	fd.io_control(cmd);
	return LineInfo(std::move(cmd.data_));
}

LineHandle::LineHandle(Ioc &ioc, Chip &chip, gpiohandle_request req)
	: fd(ioc, [&]() constexpr {
		IoControlCommand<GPIO_GET_LINEHANDLE_IOCTL, gpiohandle_request &> cmd({req});
		chip.fd.io_control(cmd);
		return cmd.data_.fd;
	}()), count(req.lines) {}

uint64_t LineHandle::get() {
	IoControlCommand<GPIOHANDLE_GET_LINE_VALUES_IOCTL, gpiohandle_data> cmd;
	fd.io_control(cmd);
	uint64_t res = 0;
	for (offset_t i = 0; i < count; ++i) {
		res |= cmd.data_.values[i] << i;
	}
	return res;
}

void LineHandle::set(uint64_t values) {
	IoControlCommand<GPIOHANDLE_SET_LINE_VALUES_IOCTL, gpiohandle_data> cmd;
	for (offset_t i = 0; i < count; ++i, values >>= 1) {
		cmd.data_.values[i] = values & 1;
	}
	fd.io_control(cmd);
}

EventHandle::EventHandle(Ioc &ioc, Chip &chip, offset_t offset
	, Edge events, std::string_view consumer, uint flags)
  : fd(ioc, [&] {
		assert(!(flags & (OpenDrain | OpenSource)));
		assert(consumer.length() < 32);
		IoControlCommand<GPIO_GET_LINEEVENT_IOCTL, gpioevent_request> cmd;
		cmd.data_.lineoffset = offset;
		cmd.data_.handleflags = In | flags;
		cmd.data_.eventflags = events;
		std::memcpy(cmd.data_.consumer_label, consumer.data(), consumer.length());
		cmd.data_.consumer_label[consumer.length()] = '\0';
		chip.fd.io_control(cmd);
		return cmd.data_.fd;
	}()) {}

Event EventHandle::read(Ec &ec) {
	ec = {};
	Event e;
	net::read(fd, net::buffer(&e.data, sizeof(e.data)), ec);
	if (ec == net::error::would_block) {
		ec = {};
		e = {};
	}
	return e;
}

Event EventHandle::read() {
	Ec ec;
	Event e = read(ec);
	if (ec) {
		throw boost::system::system_error(ec);
	}
	return e;
}

} // namespace Gpio

// vim: ts=2 sw=0 noet
