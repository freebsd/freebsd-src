# Mobile Bluetooth Stack Makefile
.PHONY: all clean

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS =

SRCS = bt_core.c
OBJS = $(SRCS:.c=.o)

all: libbtcore.a

libbtcore.a: $(OBJS)
	ar rcs $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libbtcore.a
