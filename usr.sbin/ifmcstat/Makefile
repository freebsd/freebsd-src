#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$FreeBSD$

PROG=	ifmcstat
MAN=	ifmcstat.8

BINGRP=	kmem
BINMODE=555
LDADD=	-lkvm
DPADD=	${LIBKVM}

CFLAGS+=-DINET6

.include <bsd.prog.mk>
