#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/fstat/Makefile,v 1.4.2.1 2000/07/02 10:20:24 ps Exp $

PROG=	fstat
SRCS=	cd9660.c fstat.c msdosfs.c
CFLAGS+=-Wall
CFLAGS+=-I${.CURDIR}/../../sys
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
