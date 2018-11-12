# $FreeBSD$

LIBXOSRC=	${SRCTOP}/contrib/libxo

.PATH:	${LIBXOSRC}/xo

PROG=	xo
MAN=	xo.1

# XXX For xoversion.h
CFLAGS+=-I${LIBXOSRC}/libxo

LIBADD=	xo util

.include <bsd.prog.mk>
