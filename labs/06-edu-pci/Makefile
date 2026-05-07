obj-m := edu.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean

load: all
	sudo insmod edu.ko
	sudo chmod 666 /dev/edu

unload:
	sudo rmmod edu

.PHONY: load unload clean
