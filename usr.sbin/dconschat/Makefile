# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.1 2003/10/24 15:44:10 simokawa Exp $

PROG=	dconschat
SRCS=	dconschat.c
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
