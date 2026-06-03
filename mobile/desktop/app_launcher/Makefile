CFLAGS = -Wall -Wextra -std=c99 -I../../ui -I../../services -I../app_launcher -I..
LDFLAGS = -lm

OBJS = app_launcher.o

all: libapp_launcher.a

libapp_launcher.a: $(OBJS)
	$(AR) rcs $@ $(OBJS)

app_launcher.o: app_launcher.c app_launcher.h
	$(CC) $(CFLAGS) -c app_launcher.c -o app_launcher.o

clean:
	rm -f $(OBJS) libapp_launcher.a

.PHONY: all clean
