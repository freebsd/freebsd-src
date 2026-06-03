CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = theme.o

all: libtheme.a

libtheme.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

theme.o: theme.c theme.h
	$(CC) $(CFLAGS) -c theme.c -o theme.o

clean:
	rm -f $(OBJS) libtheme.a

.PHONY: all clean
