#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/dmesg/Makefile,v 1.8.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	dmesg
MAN=	dmesg.8

WARNS?=	6

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
