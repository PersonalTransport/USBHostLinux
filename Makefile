all:
	gcc -std=c99 -g USBHostLinux.c -I/usr/include/ -o USBHostLinux -lusb-1.0 -I/usr/include/ -I/usr/include/libusb-1.0 -lncurses

clean:
	rm USBHostLinux
