#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpc/Makefile,v 1.7 1999/08/28 01:16:50 peter Exp $

PROG=	lpc
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
MAN8=	lpc.8
SRCS=	lpc.c cmds.c cmdtab.c
BINGRP=	daemon
BINMODE=2555
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	${LIBLPR} -ledit -ltermcap

.include <bsd.prog.mk>
