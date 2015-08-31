# $FreeBSD$

PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DIPSEC -DKAME_SCOPEID -DUSE_RFC2292BIS
WARNS?=	2

BINOWN=	root
BINMODE=4555

LDADD=	-lipsec -lm -lmd
DPADD= ${LIBIPSEC} ${LIBM} ${LIBMD}

.include <bsd.prog.mk>
