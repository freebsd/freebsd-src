# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.22.1.2.1 2009/10/25 01:10:29 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
