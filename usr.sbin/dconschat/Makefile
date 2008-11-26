# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.16.1 2008/10/02 02:57:24 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
