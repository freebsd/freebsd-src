# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10.32.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

