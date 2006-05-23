#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/fstat/Makefile,v 1.8 2004/06/29 21:28:09 dwmalone Exp $

PROG=	fstat
SRCS=	cd9660.c fstat.c msdosfs.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555
WARNS?=	6

.include <bsd.prog.mk>
