#	@(#)Makefile	5.5 (Berkeley) 4/23/91

PROG=	swapinfo
SRCS=	swapinfo.c devname.c getbsize.c
.PATH:  ${.CURDIR}/../../bin/df

DPADD=	${LIBMATH} ${LIBUTIL}
LDADD=	-lm -lutil
BINGRP=	kmem
BINMODE=2555

.include <bsd.prog.mk>
