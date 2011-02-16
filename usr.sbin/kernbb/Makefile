# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10.32.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

