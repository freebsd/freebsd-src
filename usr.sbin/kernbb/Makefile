# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10.26.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

