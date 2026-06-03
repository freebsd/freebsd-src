CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = dock.o

all: libdock.a

libdock.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

dock.o: dock.c dock.h
	$(CC) $(CFLAGS) -c dock.c -o dock.o

clean:
	rm -f $(OBJS) libdock.a

.PHONY: all clean
