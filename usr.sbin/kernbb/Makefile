# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10.30.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

