#	@(#)Makefile	5.2 (Berkeley) 5/13/90

PROG=	lpc
CFLAGS+=-I${.CURDIR}/../common_source
MAN8=	lpc.8
SRCS=	lpc.c cmds.c cmdtab.c startdaemon.c common.c printcap.c
BINGRP=	daemon
BINMODE=2555
.PATH:	${.CURDIR}/../common_source

.include "../../Makefile.inc"
.include <bsd.prog.mk>
