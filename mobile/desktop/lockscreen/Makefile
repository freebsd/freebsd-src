CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = lockscreen.o

all: liblockscreen.a

liblockscreen.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

lockscreen.o: lockscreen.c lockscreen.h
	$(CC) $(CFLAGS) -c lockscreen.c -o lockscreen.o

clean:
	rm -f $(OBJS) liblockscreen.a

.PHONY: all clean
