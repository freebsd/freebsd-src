#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD: src/usr.sbin/lpr/lprm/Makefile,v 1.13.32.1 2008/11/25 02:59:29 kensmith Exp $

.PATH: ${.CURDIR}/../common_source

BINDIR=	/usr/bin

PROG=	lprm
BINOWN=	root
BINGRP=	daemon
BINMODE= 6555

CFLAGS+= -I${.CURDIR}/../common_source

DPADD=	${LIBLPR}
LDADD=	${LIBLPR}

.include <bsd.prog.mk>
