#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	lpr
CFLAGS+=-I${.CURDIR}/../common_source
SRCS=	lpr.c startdaemon.c common.c
BINOWN=	root
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
.PATH:	${.CURDIR}/../common_source

.include <bsd.prog.mk>
