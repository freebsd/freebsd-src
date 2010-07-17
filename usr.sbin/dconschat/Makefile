# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2.22.1.4.1 2010/06/14 02:09:06 kensmith Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
