#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/dmesg/Makefile,v 1.8.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	dmesg
MAN=	dmesg.8

WARNS?=	6

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
