# A simple gmake Makefile, to be used on Linux and Darwin.  It shouldn't
# be used elsewhere because it assumes that the target system doesn't
# support BSD extended file flags.
#
# $FreeBSD$
#

PREFIX?=/usr/local
OWNER?=	0
GROUP?=	0

UNAME=	$(shell uname -s)

SRCS=	attrstack.c config.c detailer.c diff.c fattr.c fixups.c fnmatch.c \
	globtree.c idcache.c keyword.c lister.c main.c misc.c mux.c pathcomp.c \
	parse.c proto.c status.c stream.c threads.c token.c updater.c
OBJS=	$(SRCS:.c=.o)

WARNS=	-Wall -W -Wno-unused-parameter -Wmissing-prototypes -Wpointer-arith \
	-Wreturn-type -Wcast-qual -Wwrite-strings -Wswitch -Wshadow \
	-Wcast-align -Wunused-parameter -Wchar-subscripts -Winline \
	-Wnested-externs -Wredundant-decls -Wno-format-y2k

CFLAGS+= -g -O -pipe -DNDEBUG -I$(PREFIX)/include
ifeq ($(UNAME), Linux)
	CFLAGS+= -D_XOPEN_SOURCE -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
endif
ifeq ($(UNAME), Darwin)
	CFLAGS+= -DHAVE_FFLAGS
endif
CFLAGS+= $(WARNS)
LDFLAGS= -L$(PREFIX)/lib -lcrypto -lz -lpthread

.PHONY: all clean install

all: csup csup.1.gz

csup: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

config.c: parse.h

token.c: token.l

parse.c: parse.y

parse.h: parse.c

clean:
	rm -f csup $(OBJS) parse.c parse.h token.c csup.1.gz

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.c: %.y
	$(YACC) -d -o $@ $<

csup.1.gz: csup.1
	gzip -cn $< > $@

install: csup csup.1.gz
	install -s -o $(OWNER) -g $(GROUP) csup $(PREFIX)/bin
	install -s -o $(OWNER) -g $(GROUP) csup.1.gz $(PREFIX)/share/man/man1
