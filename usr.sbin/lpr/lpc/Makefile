#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
#	$Id: Makefile,v 1.4 1997/12/16 17:53:19 bde Exp $

PROG=	lpc
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN8=	lpc.8
SRCS=	lpc.c cmds.c cmdtab.c
BINGRP=	daemon
BINMODE=2555
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
