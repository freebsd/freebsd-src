# $FreeBSD$

.include <src.opts.mk>

LIBXOSRC=	${SRCTOP}/contrib/libxo

.PATH:	${LIBXOSRC}/xo

PROG=	xo
MAN=	xo.1

# XXX For xoversion.h
CFLAGS+=-I${LIBXOSRC}/libxo

LIBADD=	xo util

.if ${MK_TESTS} != "no"
SUBDIR+=	tests
.endif

.include <bsd.prog.mk>
