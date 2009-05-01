# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.20.1 2009/04/15 03:14:26 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
