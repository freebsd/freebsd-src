#	@(#)Makefile	8.1 (Berkeley) 6/5/93
#	$FreeBSD: src/usr.sbin/ifmcstat/Makefile,v 1.1 1999/12/28 02:37:05 shin Exp $

PROG=	ifmcstat
MAN8=	ifmcstat.8

BINGRP=	kmem
BINMODE=2555
LDADD=	-lkvm
DPADD=	${LIBKVM}

CFLAGS+=-DINET6

.include <bsd.prog.mk>
