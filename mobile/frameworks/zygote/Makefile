# Zygote Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I. -I../permissions
LDFLAGS =
LDLIBS =

SRCS = zygote.c
OBJS = $(SRCS:.c=.o)

all: libzygote.so

libzygote.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libzygote.so
