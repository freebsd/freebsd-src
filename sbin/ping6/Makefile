# $FreeBSD: src/sbin/ping6/Makefile,v 1.1.2.1 2000/07/15 07:24:47 kris Exp $

PROG=	ping6
MAN8=	ping6.8
SRCS=	ping6.c

CFLAGS+=-DINET6 -DIPSEC

BINOWN=	root
BINGRP=	bin
BINMODE=4555

LDADD=	-lipsec -lmd
DPADD= ${LIBIPSEC} ${LIBMD}

# kame scopeid hack
CFLAGS+=-DKAME_SCOPEID

.include <bsd.prog.mk>
