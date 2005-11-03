# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.2 2005/01/27 14:52:46 delphij Exp $

PROG=	dconschat
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
