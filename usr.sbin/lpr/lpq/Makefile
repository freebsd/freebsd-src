#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.3 1997/12/02 20:46:03 wollman Exp $

PROG=	lpq
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
SRCS=	lpq.c
BINOWN=	root
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
MAN1=	lpq.1
DPADD=	${.OBJDIR}/../common_source/liblpr.a
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include <bsd.prog.mk>
