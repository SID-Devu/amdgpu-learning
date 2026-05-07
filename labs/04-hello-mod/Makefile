obj-m := hello.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load:   all
	sudo insmod hello.ko who=AMD times=3

unload:
	sudo rmmod hello

dmesg:
	dmesg | tail -20
