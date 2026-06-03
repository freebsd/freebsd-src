CFLAGS = -Wall -Wextra -O2
INCLUDES = -I../../ui -I../../frameworks/app
LIBS = -L../../frameworks/app -lmobile_app

all: messages.app

messages.app: messages.c messages.h
	$(CC) $(CFLAGS) -o $@ messages.c $(INCLUDES) $(LIBS)

clean:
	rm -f messages.app

.PHONY: all clean