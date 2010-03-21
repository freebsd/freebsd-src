# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10.34.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

