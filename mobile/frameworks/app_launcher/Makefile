# App Launcher Framework

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I. -I../zygote -I../ipc -I../lifecycle -I../permissions
LDFLAGS =
LDLIBS =

SRCS = app_launcher.c
OBJS = $(SRCS:.c=.o)

all: libal.so

libal.so: $(OBJS)
	$(CC) -shared -o $@ $^ $(LDFLAGS) $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o libal.so

install: all
	cp libal.so ../../../rootfs/mobile/lib/ 2>/dev/null || true
