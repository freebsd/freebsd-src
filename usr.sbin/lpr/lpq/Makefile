#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id$

PROG=	lpq
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
SRCS=	lpq.c
BINOWN=	root
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
MAN1=	lpq.1
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include <bsd.prog.mk>
