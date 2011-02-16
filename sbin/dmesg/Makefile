#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/dmesg/Makefile,v 1.8.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	dmesg
MAN=	dmesg.8

WARNS?=	6

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
