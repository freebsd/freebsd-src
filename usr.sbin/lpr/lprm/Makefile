#	From: @(#)Makefile	8.1 (Berkeley) 6/6/93
# $FreeBSD$

.PATH: ${.CURDIR}/../common_source

BINDIR=	/usr/bin

PROG=	lprm
BINOWN=	root
BINGRP=	daemon
BINMODE= 6555

CFLAGS+= -I${.CURDIR}/../common_source

LIBADD=	lpr

.include <bsd.prog.mk>
