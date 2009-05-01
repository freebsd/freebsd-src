#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lpr/Makefile,v 1.12.34.1 2009/04/15 03:14:26 kensmith Exp $

.PATH: ${.CURDIR}/../common_source

BINDIR=	/usr/bin

PROG=	lpr
MAN=	lpr.1 printcap.5
BINOWN=	root
BINGRP=	daemon
BINMODE= 6555

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
