#	@(#)Makefile	5.6 (Berkeley) 4/23/91

PROG=	iostat
CFLAGS+=-I/sys -I${.CURDIR}/../../usr.bin/vmstat
MAN8=	iostat.8
DPADD=	${LIBUTIL}
LDADD=	-lutil
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
