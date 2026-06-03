CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = overview.o

all: liboverview.a

liboverview.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

overview.o: overview.c overview.h
	$(CC) $(CFLAGS) -c overview.c -o overview.o

clean:
	rm -f $(OBJS) liboverview.a

.PHONY: all clean
