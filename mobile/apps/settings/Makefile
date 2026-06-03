CFLAGS = -Wall -Wextra -O2
INCLUDES = -I../../ui -I../../frameworks/app
LIBS = -L../../frameworks/app -lmobile_app

all: settings.app

settings.app: settings.c settings.h
	$(CC) $(CFLAGS) -o $@ settings.c $(INCLUDES) $(LIBS)

clean:
	rm -f settings.app

.PHONY: all clean