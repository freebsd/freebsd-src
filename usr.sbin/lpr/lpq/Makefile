#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpq/Makefile,v 1.12.34.1 2009/04/15 03:14:26 kensmith Exp $

BINDIR=	/usr/bin

PROG=	lpq
BINOWN=	root
BINGRP=	daemon
BINMODE= 6555

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
