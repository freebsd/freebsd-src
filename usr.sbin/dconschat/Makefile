# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.22.1.8.1 2012/03/03 06:15:13 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
