#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	vmstat
CFLAGS+=-I${.CURDIR}/../../sys
MAN8=	vmstat.8
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
