# $FreeBSD: src/sbin/ping6/Makefile,v 1.11 2004/02/23 20:13:55 johan Exp $

PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DINET6 -DIPSEC -DKAME_SCOPEID -DUSE_RFC2292BIS \
	-DHAVE_POLL_H -DHAVE_ARC4RANDOM
WARNS?=	0

BINOWN=	root
BINMODE=4555

LDADD=	-lipsec -lm -lmd
DPADD= ${LIBIPSEC} ${LIBM} ${LIBMD}

.include <bsd.prog.mk>
