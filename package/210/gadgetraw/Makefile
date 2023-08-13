
obj-m := f_gadgetraw.o

KSRC ?= ../../../../linux-s5pv210

EXTRA_CFLAGS += -Idrivers/usb/gadget/

all:
	make -C $(KSRC) M=$(shell pwd) modules
clean:
	make -C $(KSRC) M=$(shell pwd) clean
