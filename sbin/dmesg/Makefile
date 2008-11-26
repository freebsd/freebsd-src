#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/dmesg/Makefile,v 1.8.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	dmesg
MAN=	dmesg.8

WARNS?=	6

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
