CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = panel.o

all: libpanel.a

libpanel.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

panel.o: panel.c panel.h
	$(CC) $(CFLAGS) -c panel.c -o panel.o

clean:
	rm -f $(OBJS) libpanel.a

.PHONY: all clean
