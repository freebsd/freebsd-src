# $FreeBSD$

LIBXO=	${.CURDIR:H:H}/contrib/libxo

.PATH:	${LIBXO}/xo

PROG=	xo
MAN=	xo.1

# XXX For xoversion.h
CFLAGS+=-I${LIBXO}/libxo

LIBADD=	xo util

.include <bsd.prog.mk>
