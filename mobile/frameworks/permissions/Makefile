# Permissions Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS = -lpthread

SRCS = permissions.c
OBJS = $(SRCS:.c=.o)

all: libperm.so

libperm.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libperm.so
