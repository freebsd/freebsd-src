# $FreeBSD: src/tools/regression/fstest/Makefile,v 1.1.8.1 2009/04/15 03:14:26 kensmith Exp $

CFLAGS+=-DHAS_LCHMOD
CFLAGS+=-DHAS_CHFLAGS
CFLAGS+=-DHAS_LCHFLAGS
#CFLAGS+=-DHAS_TRUNCATE64
#CFLAGS+=-DHAS_STAT64

all:	fstest

fstest:	fstest.c
	gcc -Wall ${CFLAGS} fstest.c -o fstest

clean:
	rm -f fstest
