CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = animations.o

all: libanimations.a

libanimations.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

animations.o: animations.c animations.h
	$(CC) $(CFLAGS) -c animations.c -o animations.o

clean:
	rm -f $(OBJS) libanimations.a

.PHONY: all clean
