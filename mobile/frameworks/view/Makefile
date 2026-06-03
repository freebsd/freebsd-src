# View System Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS =

SRCS = view.c
OBJS = $(SRCS:.c=.o)

all: libview.so

libview.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libview.so
