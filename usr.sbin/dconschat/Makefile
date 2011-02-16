# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.22.1.6.1 2010/12/21 17:09:25 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
