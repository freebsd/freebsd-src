# $FreeBSD: src/usr.sbin/dconschat/Makefile,v 1.1.2.1 2003/11/14 12:21:00 simokawa Exp $

PROG=	dconschat
SRCS=	dconschat.c
MAN=	dconschat.8

CFLAGS+= -I${.CURDIR}/../../sys

DPADD=  ${LIBKVM}
LDADD=  -lkvm

.include <bsd.prog.mk>
