#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpq/Makefile,v 1.7 1999/08/28 01:16:53 peter Exp $

PROG=	lpq
CFLAGS+=-I${.CURDIR}/../common_source ${CWARNFLAGS}
DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

BINGRP=	daemon
BINMODE=6555
BINDIR=	/usr/bin

.include <bsd.prog.mk>
