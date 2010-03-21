# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.24.1 2010/02/10 00:26:20 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
