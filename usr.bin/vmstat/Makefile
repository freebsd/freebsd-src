#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	vmstat
CFLAGS+=-I/sys
MAN8=	vmstat.0
BINGRP=	kmem
BINMODE=2555
DPADD=	names.c ${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
