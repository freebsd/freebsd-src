#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpr/Makefile,v 1.8 1999/08/28 01:16:54 peter Exp $

PROG=	lpr
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
MAN1=	lpr.1
MAN5=	printcap.5
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
