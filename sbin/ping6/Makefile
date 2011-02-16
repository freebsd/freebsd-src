# $FreeBSD: src/sbin/ping6/Makefile,v 1.15.10.1.6.1 2010/12/21 17:09:25 kensmith Exp $

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
