CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = panel_applets.o

all: libpanel_applets.a

libpanel_applets.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

panel_applets.o: panel_applets.c panel_applets.h
	$(CC) $(CFLAGS) -c panel_applets.c -o panel_applets.o

clean:
	rm -f $(OBJS) libpanel_applets.a

.PHONY: all clean
