# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.26.1 2010/12/21 17:10:29 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
