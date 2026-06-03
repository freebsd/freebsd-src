# Content Resolver Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS =

SRCS = content_resolver.c
OBJS = $(SRCS:.c=.o)

all: libcr.so

libcr.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libcr.so
