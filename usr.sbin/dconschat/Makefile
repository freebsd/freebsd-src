# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.18.1 2008/11/25 02:59:29 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
