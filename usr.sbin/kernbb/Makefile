# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10.28.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

