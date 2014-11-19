# $FreeBSD$

LIBXO=	${.CURDIR:H:H}/contrib/libxo

.PATH:	${LIBXO}/xo

PROG=	xo
MAN=	xo.1

# XXX For xoversion.h
CFLAGS+=-I${LIBXO}/libxo

DPADD=	${LIBXO}
LDADD=	-lxo

.include <bsd.prog.mk>
