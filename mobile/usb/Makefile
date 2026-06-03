# Mobile USB Subsystem Makefile
.PHONY: all clean

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS =

SRCS = usb.c
OBJS = $(SRCS:.c=.o)

all: libusb.a

libusb.a: $(OBJS)
	ar rcs $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libusb.a
