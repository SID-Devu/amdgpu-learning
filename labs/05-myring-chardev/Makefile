obj-m := myring.o

KDIR := /lib/modules/$(shell uname -r)/build
PWD  := $(shell pwd)

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	$(MAKE) -f Makefile.user

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	$(MAKE) -f Makefile.user clean

load: all
	sudo insmod myring.ko
	sudo chmod 666 /dev/myring

unload:
	sudo rmmod myring

.PHONY: load unload clean
