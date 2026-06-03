CFLAGS = -Wall -Wextra -O2
INCLUDES = -I../../ui -I../../frameworks/app
LIBS = -L../../frameworks/app -lmobile_app

all: terminal.app

terminal.app: terminal.c terminal.h
	$(CC) $(CFLAGS) -o $@ terminal.c $(INCLUDES) $(LIBS)

clean:
	rm -f terminal.app

.PHONY: all clean