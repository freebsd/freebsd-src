# $FreeBSD$

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
