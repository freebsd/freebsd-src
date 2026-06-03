# Mobile Sensors Makefile
.PHONY: all clean

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS =

SRCS = sensor.c accelerometer.c gyroscope.c sensor_fusion.c gps.c
OBJS = $(SRCS:.c=.o)

all: libsensors.a

libsensors.a: $(OBJS)
	ar rcs $@ $(OBJS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libsensors.a
