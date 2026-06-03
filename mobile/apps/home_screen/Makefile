CFLAGS = -Wall -Wextra -O2
INCLUDES = -I../../ui -I../../frameworks/app
LIBS = -L../../frameworks/app -lmobile_app

all: home_screen.app

home_screen.app: home_screen.c home_screen.h
	$(CC) $(CFLAGS) -o $@ home_screen.c $(INCLUDES) $(LIBS)

clean:
	rm -f home_screen.app

.PHONY: all clean