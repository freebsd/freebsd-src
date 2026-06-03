# Lifecycle Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS = -lpthread

SRCS = lifecycle.c
OBJS = $(SRCS:.c=.o)

all: liblifecycle.so

liblifecycle.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o liblifecycle.so
