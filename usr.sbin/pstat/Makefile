#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	pstat
CFLAGS+=-I${.CURDIR}/../../sys
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM}
LDADD=	-lkvm
MAN8=	pstat.8
LINKS=	${BINDIR}/pstat ${BINDIR}/swapinfo
MLINKS=	pstat.8 swapinfo.8

.include <bsd.prog.mk>
