# clone libopencm3 into this directory from https://github.com/libopencm3/libopencm3
OPENCM3_DIR = libopencm3
DEVICE = STM32F042C6
OBJS += main.o

CFLAGS += -Os -ggdb3
CPPFLAGS += -MD
LDFLAGS += -static -nostartfiles
LDLIBS += -Wl,--start-group -lc -lgcc -lnosys -Wl,--end-group

include $(OPENCM3_DIR)/mk/genlink-config.mk
include $(OPENCM3_DIR)/mk/gcc-config.mk

.PHONY: all info clean

all: binary.elf binary.bin

# print device info using st-link v2
info:
	st-info --probe

# flash using st-link v2
flash: binary.bin
	st-flash write binary.bin 0x8000000

# flash using builtin dfu bootloader: Set jumber and connect device to usb
dfu: binary.bin
	dfu-util -a 0 -s 0x08000000 -D binary.bin

clean:
	$(Q)$(RM) -rf binary.* *.o

include $(OPENCM3_DIR)/mk/genlink-rules.mk
include $(OPENCM3_DIR)/mk/gcc-rules.mk
