#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	iostat
CFLAGS+=-I/sys -I${.CURDIR}/../../usr.bin/vmstat
MAN8=	iostat.0
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
