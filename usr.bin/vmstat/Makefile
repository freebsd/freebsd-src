#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	vmstat
MAN=	vmstat.8
CFLAGS+=-I${.CURDIR}/../../sys
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM} ${LIBDEVSTAT}
LDADD=	-lkvm -ldevstat

.include <bsd.prog.mk>
