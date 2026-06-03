# IPC Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I. -I../app_launcher -I../zygote
LDFLAGS =
LDLIBS = -lpthread

SRCS = ipc.c
OBJS = $(SRCS:.c=.o)

all: libipc.so

libipc.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libipc.so
