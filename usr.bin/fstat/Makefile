#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.bin/fstat/Makefile,v 1.9.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	fstat
SRCS=	cd9660.c fstat.c msdosfs.c
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555
WARNS?=	6

CFLAGS+=-D_KVM_VNODE

.include <bsd.prog.mk>
