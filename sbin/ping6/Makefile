# $FreeBSD: src/sbin/ping6/Makefile,v 1.1.2.3 2001/09/30 22:35:26 kris Exp $

PROG=	ping6
MAN=	ping6.8

CFLAGS+=-DINET6 -DIPSEC

BINOWN=	root
BINMODE=4555

LDADD=	-lipsec -lmd
DPADD= ${LIBIPSEC} ${LIBMD}

# kame scopeid hack
CFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
