#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/fstat/Makefile,v 1.2.2.2 1999/08/29 15:28:06 peter Exp $

PROG=	fstat
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../../sys
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
