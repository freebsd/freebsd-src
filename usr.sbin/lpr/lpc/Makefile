#	@(#)Makefile	8.1 (Berkeley) 6/6/93

PROG=	lpc
CFLAGS+=-I${.CURDIR}/../common_source
MAN8=	lpc.0
SRCS=	lpc.c cmds.c cmdtab.c startdaemon.c common.c
BINGRP=	daemon
BINMODE=2555
.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
