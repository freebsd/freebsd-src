# $FreeBSD: src/sbin/ping6/Makefile,v 1.15.12.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DIPSEC -DKAME_SCOPEID -DUSE_RFC2292BIS \
	-DHAVE_POLL_H -DHAVE_ARC4RANDOM
WARNS?=	2

BINOWN=	root
BINMODE=4555

LDADD=	-lipsec -lm -lmd
DPADD= ${LIBIPSEC} ${LIBM} ${LIBMD}

.include <bsd.prog.mk>
