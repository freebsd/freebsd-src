# Sandbox Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I. -I../permissions
LDFLAGS =
LDLIBS =

SRCS = sandbox.c
OBJS = $(SRCS:.c=.o)

all: libsandbox.so

libsandbox.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libsandbox.so
