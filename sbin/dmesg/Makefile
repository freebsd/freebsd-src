#	@(#)Makefile	8.1 (Berkeley) 6/5/93
# $FreeBSD: src/sbin/dmesg/Makefile,v 1.8 2005/01/17 13:56:46 delphij Exp $

PROG=	dmesg
MAN=	dmesg.8

WARNS?=	6

LDADD=	-lkvm
DPADD=	${LIBKVM}

.include <bsd.prog.mk>
