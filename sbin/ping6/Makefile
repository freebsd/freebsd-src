# $FreeBSD$

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
