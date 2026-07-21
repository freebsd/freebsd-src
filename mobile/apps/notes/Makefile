CFLAGS = -Wall -Wextra -O2
INCLUDES = -I../../ui -I../../frameworks/app

all: notes.app

notes.app: notes.c
	$(CC) $(CFLAGS) -o $@ notes.c $(INCLUDES)

clean:
	rm -f notes.app

.PHONY: all clean
