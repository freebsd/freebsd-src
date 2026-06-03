CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services
LDFLAGS = -lm

OBJS = icon_theme.o

all: libicon_theme.a

libicon_theme.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

icon_theme.o: icon_theme.c icon_theme.h
	$(CC) $(CFLAGS) -c icon_theme.c -o icon_theme.o

clean:
	rm -f $(OBJS) libicon_theme.a

.PHONY: all clean
