CC      := gcc
PKGS    := libdrm_amdgpu libdrm
CFLAGS  := -std=c11 -Wall -Wextra -O2 -g $(shell pkg-config --cflags $(PKGS))
LDLIBS  := $(shell pkg-config --libs $(PKGS))

info: info.c
	$(CC) $(CFLAGS) info.c $(LDLIBS) -o info

run: info
	./info

clean:
	rm -f info

.PHONY: run clean
