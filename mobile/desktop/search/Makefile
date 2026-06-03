CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I..
LDFLAGS = -lm

OBJS = search.o

all: libsearch.a

libsearch.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

search.o: search.c search.h
	$(CC) $(CFLAGS) -c search.c -o search.o

clean:
	rm -f $(OBJS) libsearch.a

.PHONY: all clean
