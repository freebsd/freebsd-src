# Mobile Camera Makefile
.PHONY: all clean

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS =

SRCS = camera.c camera2.c camera_mock.c
OBJS = $(SRCS:.c=.o)

all: libcamera.a

libcamera.a: $(OBJS)
	ar rcs $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libcamera.a
