# $FreeBSD$

PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DINET6 -DIPSEC
WARNS=	0

BINOWN=	root
BINMODE=4555

LDADD=	-lipsec -lmd
DPADD= ${LIBIPSEC} ${LIBMD}

# kame scopeid hack
CFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
