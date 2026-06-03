CC ?= cc
CFLAGS ?= -Wall -Wextra -O2
LDFLAGS ?=
PREFIX ?= /usr/local

# Check for sqlite3 library
SQLITE3_CHECK := $(shell $(CC) -lsqlite3 -o /dev/null -xc - <<<"#include <sqlite3.h>int main(){return 0;}" 2>/dev/null; echo $$?)
ifeq ($(SQLITE3_CHECK),0)
    LIBS = -lsqlite3
else
    LIBS =
    $(warning sqlite3 not found, building without SQLite support)
endif

all: pkgctl

pkgctl: pkgctl.o libpkg.o
	$(CC) $(LDFLAGS) -o $@ pkgctl.o libpkg.o $(LIBS)

pkgctl.o: pkgctl.c libpkg.h
	$(CC) $(CFLAGS) -c pkgctl.c

libpkg.o: libpkg.c libpkg.h
	$(CC) $(CFLAGS) -c libpkg.c

clean:
	rm -f pkgctl pkgctl.o libpkg.o

install: all
	install -d $(DESTDIR)$(PREFIX)/bin
	install -d $(DESTDIR)$(PREFIX)/man/man1
	install -m 555 pkgctl $(DESTDIR)$(PREFIX)/bin/
	install -m 444 pkgctl.1 $(DESTDIR)$(PREFIX)/man/man1/

.PHONY: all clean install