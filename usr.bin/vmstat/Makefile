#	@(#)Makefile	5.7 (Berkeley) 4/24/91

PROG=	vmstat
CFLAGS+=-I/sys
MAN8=	vmstat.8
BINGRP=	kmem
BINMODE=2555
DPADD=	names.c ${LIBUTIL}
LDADD=	-lutil

.include <bsd.prog.mk>
