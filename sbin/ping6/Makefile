# $FreeBSD: src/sbin/ping6/Makefile,v 1.1.2.4 2002/03/08 09:18:59 ume Exp $

PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DINET6 -DIPSEC

BINOWN=	root
BINMODE=4555

LDADD=	-lipsec -lm -lmd
DPADD= ${LIBIPSEC} ${LIBM} ${LIBMD}

# kame scopeid hack
CFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
