#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpr/Makefile,v 1.8.2.1 2001/04/25 12:09:54 ru Exp $

PROG=	lpr
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin
MAN=	lpr.1 printcap.5
.PATH:	${.CURDIR}/../common_source
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
