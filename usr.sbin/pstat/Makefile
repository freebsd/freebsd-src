#	@(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

PROG=	pstat
CFLAGS+=-I${.CURDIR}/../../sys
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM}
LDADD=	-lkvm
LINKS=	${BINDIR}/pstat ${BINDIR}/swapinfo
MLINKS=	pstat.8 swapinfo.8

.include <bsd.prog.mk>
