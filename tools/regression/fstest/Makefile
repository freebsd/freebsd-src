# $FreeBSD: src/tools/regression/fstest/Makefile,v 1.1.12.1 2010/12/21 17:10:29 kensmith Exp $

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
