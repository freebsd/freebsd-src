# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10.32.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

