#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.3 1997/12/02 20:45:35 wollman Exp $

PROG=	lpc
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN8=	lpc.8
SRCS=	lpc.c cmds.c cmdtab.c
BINGRP=	daemon
BINMODE=2555
.PATH:	${.CURDIR}/../common_source
DPADD=	${.OBJDIR}/../common_source/liblpr.a
LDADD=	-L${.OBJDIR}/../common_source -llpr

.include "../../Makefile.inc"
.include <bsd.prog.mk>
