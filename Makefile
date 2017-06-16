
nanopi-load: main.c s5p_boot.h s5p_usb.h
	gcc -Wall -Wextra -Os main.c $$(pkg-config --cflags --libs libusb-1.0) \
		-o nanopi-load

clean:
	rm -f nanopi-load *.o

