gpio-utils = gpio_event gpio_read gpio_write
gpio-utils: $(gpio-utils)
.PHONY: gpio-utils

build/libgpio.a: gpio.cpp.o -lpthread

gpio_event: event.cpp.o -lgpio
gpio_read:   read.cpp.o -lgpio
gpio_write: write.cpp.o -lgpio

ifndef mainMakefile
mainMakefile = 1
CXXSTD = c++17
clean: ; rm -rf build $(gpio-utils)
include .mk
endif
