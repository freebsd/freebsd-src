#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id$

PROG=	lpr
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
SRCS=	lpr.c
BINOWN=	root
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
MAN1=	lpr.1
.PATH:	${.CURDIR}/../common_source
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include <bsd.prog.mk>
