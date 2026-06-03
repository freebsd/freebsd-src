# Notification Manager Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I.
LDFLAGS =
LDLIBS =

SRCS = notification_mgr.c
OBJS = $(SRCS:.c=.o)

all: libnm.so

libnm.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libnm.so
