CFLAGS = -Wall -Wextra -O2
INCLUDES = -I../../ui -I../../frameworks/app
LIBS = -L../../frameworks/app -lmobile_app

all: browser.app

browser.app: browser.c browser.h
	$(CC) $(CFLAGS) -o $@ browser.c $(INCLUDES) $(LIBS)

clean:
	rm -f browser.app

.PHONY: all clean