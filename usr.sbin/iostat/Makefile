#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	iostat
CFLAGS+=-I${.CURDIR}/../../usr.bin/vmstat -I${.CURDIR}/../../sys
MAN8=	iostat.8
DPADD=	${LIBKVM}
LDADD=	-lkvm
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
