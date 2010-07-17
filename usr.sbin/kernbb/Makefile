# $FreeBSD: src/usr.sbin/kernbb/Makefile,v 1.10.32.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	kernbb
MAN=	kernbb.8

DPADD=	${LIBKVM}
LDADD=	-lkvm

CFLAGS+= -I${.CURDIR}/../../contrib/gcc

WARNS?=	3
.include <bsd.prog.mk>

