CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = notifications.o

all: libnotifications.a

libnotifications.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

notifications.o: notifications.c notifications.h
	$(CC) $(CFLAGS) -c notifications.c -o notifications.o

clean:
	rm -f $(OBJS) libnotifications.a

.PHONY: all clean
