#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	pstat
CFLAGS+=-I/sys
BINGRP=	kmem
BINMODE=2555
DPADD=	${LIBKVM}
LDADD=	-lkvm

.include <bsd.prog.mk>
