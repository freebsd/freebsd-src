#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id$

PROG=	lprm
CFLAGS+=-I${.CURDIR}/../common_source -Wall -Werror
SRCS=	lprm.c
BINOWN=	root
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
MAN1=	lprm.1
.PATH:	${.CURDIR}/../common_source
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include <bsd.prog.mk>
