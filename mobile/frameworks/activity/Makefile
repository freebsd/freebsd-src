# Activity Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I. -I../app_launcher -I../lifecycle -I../ipc
LDFLAGS =
LDLIBS =

SRCS = activity.c
OBJS = $(SRCS:.c=.o)

all: libactivity.so

libactivity.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libactivity.so
