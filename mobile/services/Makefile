# Mobile Services Makefile
# Builds each service as a standalone binary

.PHONY: all clean install

CC = cc
CFLAGS = -Wall -Wextra -O2 -g -I. -I../ui -I../kernel -I../pkg
LDFLAGS =
LDLIBS =

SRCS = init.c displayd.c powerd.c networkd.c audiod.c timed.c service_mgr.c pkgd.c
OBJS = $(SRCS:.c=.o)

all: bin/init bin/displayd bin/powerd bin/networkd bin/audiod bin/timed bin/pkgd

mkdirs:
	@mkdir -p bin

bin/init: init.o service_mgr.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

bin/displayd: displayd.o service_mgr.o
	$(CC) $(LDFLAGS) -o $@ $^ -I../ui $(LDLIBS)

bin/powerd: powerd.o service_mgr.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

bin/networkd: networkd.o service_mgr.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

bin/audiod: audiod.o service_mgr.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

bin/timed: timed.o service_mgr.o
	$(CC) $(LDFLAGS) -o $@ $^ $(LDLIBS)

bin/pkgd: pkgd.o service_mgr.o
	$(CC) $(LDFLAGS) -o $@ $^ -I../pkg $(LDLIBS)

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o
	rm -f bin/*

install: all
	cp bin/* ../../rootfs/mobile/bin/ 2>/dev/null || true
	@echo "Services installed to rootfs/mobile/bin/"
