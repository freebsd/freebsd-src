CFLAGS = -Wall -Wextra -O2
INCLUDES = -I../../ui -I../../frameworks/app
LIBS = -L../../frameworks/app -lmobile_app

all: contacts.app

contacts.app: contacts.c contacts.h
	$(CC) $(CFLAGS) -o $@ contacts.c $(INCLUDES) $(LIBS)

clean:
	rm -f contacts.app

.PHONY: all clean