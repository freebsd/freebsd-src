CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services
LDFLAGS = -lm

OBJS = wallpaper.o

all: libwallpaper.a

libwallpaper.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

wallpaper.o: wallpaper.c wallpaper.h
	$(CC) $(CFLAGS) -c wallpaper.c -o wallpaper.o

clean:
	rm -f $(OBJS) libwallpaper.a

.PHONY: all clean
