#	@(#)Makefile	5.3 (Berkeley) 6/19/90

PROG=	lpr
CFLAGS+=-I${.CURDIR}/../common_source
SRCS=	lpr.c startdaemon.c printcap.c
BINOWN=	root
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
.PATH:	${.CURDIR}/../common_source

.include <bsd.prog.mk>
