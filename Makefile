
CROSS_COMPILE=/home/kls/buildroot/buildroot-2023.08/output/host/bin/arm-buildroot-linux-gnueabihf-
CC = $(CROSS_COMPILE)gcc
CFLAGS = -Wall

all: homeoffice

homeoffice: homeoffice.c
	$(CC) $(CFLAGS) -o $@ $^

install: homeoffice
	cp $< $(TARGET_DIR)/usr/bin
	install -m 755 $(@D)/S99kernelmodules $(TARGET_DIR)/etc/init.d/S99kernelmodules

clean:
	rm -f homeoffice