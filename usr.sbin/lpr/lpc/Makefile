#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpc/Makefile,v 1.12 2002/07/17 00:51:19 gad Exp $

.PATH: ${.CURDIR}/../common_source

PROG=	lpc
MAN=	lpc.8
SRCS=	lpc.c cmds.c cmdtab.c movejobs.c 
BINGRP=	daemon
BINMODE= 2555

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR} ${LIBEDIT} ${LIBTERMCAP}
LDADD=	${LIBLPR} -ledit -ltermcap

.include <bsd.prog.mk>
